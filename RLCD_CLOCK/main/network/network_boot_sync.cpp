// 执行启动页 Wi-Fi 连接与时间校准，页面 HTTPS 数据统一留给后台错峰同步。
#include "network_boot_sync.h"

#include "app_event_group.h"
#include "app_metadata.h"
#include "app_text_format.h"
#include "network_credentials_state.h"
#include "ntp_services.h"
#include "offline_mode_state.h"
#include "network_sync_schedule.h"
#include "network_task_guards.h"
#include "sensor_time.h"
#include "ui_boot_screen.h"
#include "wifi_portal_state.h"
#include "wifi_radio_services_internal.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

#include <stdio.h>
#include <string.h>

namespace {
static portMUX_TYPE s_boot_sync_deadline_mux = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_boot_sync_deadline_us = 0;
constexpr int64_t kMicrosecondsPerMillisecond = 1000;
constexpr uint32_t kBootScreenShortDelayMs = 200;
constexpr uint32_t kBootScreenOfflineDelayMs = 600;
constexpr uint32_t kBootScreenSetupDelayMs = 1500;
constexpr int kBootScreenCompletePercent = 100;
constexpr int kBootNtpMinRemainingMs = 600;
constexpr size_t kBootSetupDetailTextSize = 64;
constexpr const char *kBootDetailStartingClock = "Starting clock";
constexpr const char *kBootDetailPowerLockUnavailable = "Power lock unavailable";
constexpr const char *kBootDetailSynchronizingTime = "Synchronizing time";
constexpr const char *kBootDetailPageDataQueued = "Page data queued";
constexpr const char *kBootDetailBackgroundRefresh = "Refreshing after startup";
constexpr const char *kBootSetupDetailFallback = "Setup AP: --";
constexpr const char *kBootSetupDetailFormat = "Setup AP: %s";
constexpr const char *kBootRtcInvalidNtpPriorityLog =
    "system time invalid after Wi-Fi connect, prioritizing boot NTP";
constexpr const char *kBootPageDataDeferredLog =
    "boot page HTTPS deferred to staggered background sync";

static int64_t load_boot_sync_deadline_us()
{
    portENTER_CRITICAL(&s_boot_sync_deadline_mux);
    const int64_t deadline_us = s_boot_sync_deadline_us;
    portEXIT_CRITICAL(&s_boot_sync_deadline_mux);
    return deadline_us;
}

static void store_boot_sync_deadline_us(int64_t deadline_us)
{
    portENTER_CRITICAL(&s_boot_sync_deadline_mux);
    s_boot_sync_deadline_us = deadline_us;
    portEXIT_CRITICAL(&s_boot_sync_deadline_mux);
}

class BootSyncDeadlineGuard {
public:
    BootSyncDeadlineGuard()
    {
        store_boot_sync_deadline_us(
            esp_timer_get_time() +
            static_cast<int64_t>(kBootStartupBudgetMs) *
                kMicrosecondsPerMillisecond);
    }

    ~BootSyncDeadlineGuard()
    {
        store_boot_sync_deadline_us(0);
    }

    BootSyncDeadlineGuard(const BootSyncDeadlineGuard &) = delete;
    BootSyncDeadlineGuard &operator=(const BootSyncDeadlineGuard &) = delete;
};

void copy_boot_detail_fallback_on_format_error(int written, char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    if (app_text::format_failed(written, out_len)) {
        strlcpy(out, kBootSetupDetailFallback, out_len);
    }
}

void format_boot_setup_detail(char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    char setup_ap_ssid[kWifiSetupApSsidTextLen] = {};
    (void)wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid));
    int written = snprintf(out, out_len, kBootSetupDetailFormat, setup_ap_ssid);
    copy_boot_detail_fallback_on_format_error(written, out, out_len);
}

void finish_boot_network_session(NetworkAwakeLockGuard &awake_lock)
{
    stop_wifi_radio();
    awake_lock.release();
    request_wifi_radio_stop_if_running();
}

} // namespace

int boot_sync_remaining_ms()
{
    return network_boot_budget_remaining_ms(load_boot_sync_deadline_us(),
                                            esp_timer_get_time());
}

void run_boot_connectivity_sync()
{
    if (offline_mode_enabled_load()) {
        update_boot_screen(kBootScreenCompletePercent, "Offline mode", "Using RTC time");
        vTaskDelay(pdMS_TO_TICKS(kBootScreenOfflineDelayMs));
        return;
    }
    char wifi_ssid[kNetworkWifiSsidLen] = {};
    if (!network_all_online_credentials_configured() ||
        !network_wifi_ssid_snapshot(wifi_ssid, sizeof(wifi_ssid))) {
        char detail[kBootSetupDetailTextSize] = {};
        format_boot_setup_detail(detail, sizeof(detail));
        update_boot_screen(kBootScreenCompletePercent, "Setup mode", detail);
        vTaskDelay(pdMS_TO_TICKS(kBootScreenSetupDelayMs));
        return;
    }

    update_boot_screen(18, "Connecting Wi-Fi", wifi_ssid);
    NetworkAwakeLockGuard awake_lock;
    BootSyncDeadlineGuard deadline_guard;
    if (!awake_lock.locked()) {
        update_boot_screen(kBootScreenCompletePercent,
                           kBootDetailPowerLockUnavailable,
                           kBootDetailStartingClock);
        service_wifi_radio_stop_when_idle();
        vTaskDelay(pdMS_TO_TICKS(kBootScreenShortDelayMs));
        return;
    }
    if (!start_wifi_radio(false)) {
        update_boot_screen(kBootScreenCompletePercent, "Wi-Fi start failed", kBootDetailStartingClock);
        awake_lock.release();
        // A running radio can fail while being reconfigured. Register a real
        // close request after releasing this session's PM lock so that rare
        // partial-start failures cannot leave Wi-Fi powered indefinitely.
        request_wifi_radio_stop_if_running();
        vTaskDelay(pdMS_TO_TICKS(kBootScreenShortDelayMs));
        return;
    }
    int remaining_ms = boot_sync_remaining_ms();
    uint32_t wifi_timeout_ms = remaining_ms > 0 && remaining_ms < kBootWifiConnectTimeoutMs
                                   ? remaining_ms
                                   : kBootWifiConnectTimeoutMs;
    if (!wait_for_wifi_connected(wifi_timeout_ms)) {
        update_boot_screen(kBootScreenCompletePercent, "Wi-Fi timeout", "Check SSID or password");
        finish_boot_network_session(awake_lock);
        vTaskDelay(pdMS_TO_TICKS(kBootScreenShortDelayMs));
        return;
    }

    update_boot_screen(42, "Wi-Fi connected", "Checking time");
    ESP_LOGI(TAG, "%s", kBootPageDataDeferredLog);
    bool ntp_attempted = false;
    bool ntp_ok = false;
    if (!is_system_time_plausible()) {
        ESP_LOGI(TAG, "%s", kBootRtcInvalidNtpPriorityLog);
        remaining_ms = boot_sync_remaining_ms();
        if (remaining_ms > kBootNtpMinRemainingMs) {
            ntp_attempted = true;
            update_boot_screen(46, kBootDetailSynchronizingTime, "Restoring lost RTC time");
            ntp_ok = perform_ntp_sync(kBootNtpRetries);
            update_boot_screen(72,
                               ntp_ok ? "Time synchronized" : "NTP retry later",
                               ntp_ok ? kBootDetailPageDataQueued : "Will retry in background");
        }
    }
    remaining_ms = boot_sync_remaining_ms();
    if (!ntp_attempted && remaining_ms > kBootNtpMinRemainingMs) {
        update_boot_screen(82, kBootDetailSynchronizingTime, "Short NTP check");
        ntp_ok = perform_ntp_sync(kBootNtpRetries);
    }
    update_boot_screen(kBootScreenCompletePercent,
                       ntp_ok ? "Time synchronized" : "NTP retry later",
                       kBootDetailBackgroundRefresh);

    finish_boot_network_session(awake_lock);
    vTaskDelay(pdMS_TO_TICKS(kBootScreenShortDelayMs));
}

void boot_connectivity_task(void *)
{
    run_boot_connectivity_sync();
    app_event_group_set_bits(kBootSyncDoneBit);
    vTaskDelete(nullptr);
}
