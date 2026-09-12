// 实现小智实时会话与待唤醒阶段的网络和电源所有权切换。
#include "xiaozhi_power_session.h"

#include "app_metadata.h"
#include "app_event_group.h"
#include "audio_services.h"
#include "power_services.h"
#include "wifi_radio_services.h"
#include "wifi_radio_state.h"

#include <atomic>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_wifi.h"

namespace {
constexpr uint32_t kWifiWaitMs = 30000;
constexpr int kWifiPowerSaveMaxAttempts = 3;
constexpr TickType_t kWifiPowerSaveRetryDelay = pdMS_TO_TICKS(10);

std::atomic<bool> s_network_keepalive{false};
bool s_network_lock_held = false;
bool s_idle_low_power = false;

bool set_wifi_power_save_with_retry(wifi_ps_type_t mode, const char *operation)
{
    // MiPlay/airkan 常驻发现期间 modem sleep 必须保持关闭（组播可达性）。
    // xiaozhi 会话结束/空闲时想把 PS 切回 MAX_MODEM 的路径在此统一拦截，
    // 拦截后向调用方报告成功——电源策略归属是全局决定，不是失败。
    if (mode == WIFI_PS_MAX_MODEM && wifi_always_on_required()) {
        ESP_LOGD(TAG,
                 "Xiaozhi %s skipped: discovery services require no modem sleep",
                 operation);
        return true;
    }
    esp_err_t err = ESP_FAIL;
    for (int attempt = 1; attempt <= kWifiPowerSaveMaxAttempts; ++attempt) {
        err = esp_wifi_set_ps(mode);
        if (err == ESP_OK) {
            return true;
        }
        if (attempt < kWifiPowerSaveMaxAttempts) {
            vTaskDelay(kWifiPowerSaveRetryDelay);
        }
    }
    ESP_LOGW(TAG,
             "Xiaozhi %s failed after %d attempts: %s",
             operation,
             kWifiPowerSaveMaxAttempts,
             esp_err_to_name(err));
    return false;
}
} // namespace

bool xiaozhi_power_session_keepalive_active()
{
    return s_network_keepalive.load(std::memory_order_acquire);
}

bool xiaozhi_power_session_task_start_blocked()
{
    return network_awake_lock_active();
}

XiaozhiPowerSessionSnapshot xiaozhi_power_session_snapshot()
{
    return {
        xiaozhi_power_session_keepalive_active(),
        s_network_lock_held,
        s_idle_low_power,
    };
}

void xiaozhi_power_session_release()
{
    s_idle_low_power = false;
    if (xiaozhi_power_session_keepalive_active()) {
        // A competing network owner can keep the radio alive after this page
        // releases its lock. Restore the normal low-power policy before
        // publishing keepalive=false so that deferred shutdown never leaves
        // Wi-Fi in the realtime session's high-power mode.
        (void)set_wifi_power_save_with_retry(WIFI_PS_MAX_MODEM,
                                             "release Wi-Fi power save");
        s_network_keepalive.store(false, std::memory_order_release);
    }
    if (s_network_lock_held) {
        release_network_awake_lock();
        s_network_lock_held = false;
    }
    // Other weather or diagnostics work may still own the shared network lock.
    // Try to close immediately; the radio module wakes the serialized network
    // task only when another owner or a driver error requires deferred cleanup.
    request_wifi_radio_stop_when_idle();
}

bool xiaozhi_power_session_set_idle(bool enabled)
{
    if (enabled == s_idle_low_power) {
        // Rebuilding WakeNet starts a new audio session and reacquires CPU MAX.
        // Reconcile the lock even when the logical power state did not change.
        return set_xiaozhi_audio_high_performance(!enabled);
    }
    if (enabled) {
        if (!set_wifi_power_save_with_retry(WIFI_PS_MAX_MODEM,
                                            "idle Wi-Fi power save")) {
            return false;
        }
        if (!set_xiaozhi_audio_high_performance(false)) {
            (void)set_wifi_power_save_with_retry(WIFI_PS_NONE,
                                                 "idle rollback Wi-Fi power save");
            return false;
        }
        if (s_network_lock_held) {
            release_network_awake_lock();
            s_network_lock_held = false;
        }
        s_idle_low_power = true;
        ESP_LOGI(TAG, "Xiaozhi wake idle power: CPU DFS + Wi-Fi max modem sleep");
        return true;
    }
    bool network_lock_acquired = false;
    if (!s_network_lock_held) {
        if (!acquire_network_awake_lock()) {
            ESP_LOGW(TAG, "Xiaozhi network PM lock unavailable");
            return false;
        }
        s_network_lock_held = true;
        network_lock_acquired = true;
    }
    if (!set_wifi_power_save_with_retry(WIFI_PS_NONE,
                                        "realtime Wi-Fi power save disable")) {
        if (network_lock_acquired) {
            release_network_awake_lock();
            s_network_lock_held = false;
        }
        return false;
    }
    if (!set_xiaozhi_audio_high_performance(true)) {
        (void)set_wifi_power_save_with_retry(WIFI_PS_MAX_MODEM,
                                             "realtime rollback Wi-Fi power save");
        if (network_lock_acquired) {
            release_network_awake_lock();
            s_network_lock_held = false;
        }
        return false;
    }
    s_idle_low_power = false;
    ESP_LOGI(TAG, "Xiaozhi realtime power restored");
    return true;
}

bool xiaozhi_power_session_acquire_realtime()
{
    bool connected = app_event_group_ready() &&
                     ((app_event_group_get_bits() & kWifiConnectedBit) != 0);
    if (s_idle_low_power && wifi_radio_on_load() && connected) {
        return true;
    }
    if (s_idle_low_power && !xiaozhi_power_session_set_idle(false)) {
        return false;
    }
    if (!s_network_lock_held) {
        if (!acquire_network_awake_lock()) {
            ESP_LOGW(TAG, "Xiaozhi network PM lock unavailable");
            return false;
        }
        s_network_lock_held = true;
    }
    // Always cross the serialized radio lifecycle boundary. A deferred close
    // may already be in flight after this session acquires its PM lock; the
    // start call then either makes that close defer or restarts the radio after
    // the old owner finishes stopping it.
    if (!start_wifi_radio(false)) {
        return false;
    }
    // Once the page owns keepalive, only publish the transition once. Repeating
    // esp_wifi_set_ps() produces noisy logs and unnecessary driver work.
    if (!xiaozhi_power_session_keepalive_active()) {
        if (!set_wifi_power_save_with_retry(WIFI_PS_NONE,
                                            "initial realtime Wi-Fi power save disable")) {
            return false;
        }
        s_network_keepalive.store(true, std::memory_order_release);
    }
    return wait_for_wifi_connected(kWifiWaitMs, kXiaozhiPageStateChangedBit);
}
