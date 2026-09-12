// 实现配网热点延后启动、结果页显示宽限和失败门户保留事务。
#include "network_provisioning_session_internal.h"

#include "app_event_group.h"
#include "app_metadata.h"
#include "app_tick_time.h"
#include "network_diagnostics_state.h"
#include "network_task_guards.h"
#include "setup_portal_control.h"
#include "setup_portal_control_internal.h"
#include "ui_info_page_state.h"
#include "ui_settings_activity_state.h"
#include "ui_task_notify.h"
#include "wifi_radio_services.h"
#include "wifi_radio_state.h"
#include "wifi_portal_state_internal.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr uint32_t kProvisioningFeedbackWaitMs = 30000;
constexpr uint32_t kProvisioningFeedbackDisplayGraceMs = 750;
static_assert(kProvisioningFeedbackWaitMs >= kProvisioningFeedbackDisplayGraceMs,
              "provisioning feedback wait must cover its display grace");
constexpr const char *kProvisioningValidationFailedKeepPortalLog =
    "provisioning validation failed; setup portal remains active";
constexpr const char *kSetupPortalStartFailedLog =
    "queued setup portal start failed; retrying";
constexpr const char *kSetupPortalStartedLog =
    "queued setup portal start completed";
constexpr const char *kSetupPortalStopFailedLog =
    "queued setup portal stop failed; retrying";
constexpr const char *kSetupPortalStoppedLog =
    "queued setup portal stop completed";
#define PROVISIONING_FEEDBACK_WAIT_DONE_FORMAT \
    "provisioning result feedback wait complete: seen=%d current=%d elapsed_ms=%u"
#define PROVISIONING_RESULT_DELIVERY_DEGRADED_FORMAT \
    "setup portal AP-only result delivery unavailable; publishing result"
constexpr const char *kProvisioningAttemptSupersededLog =
    "provisioning validation superseded by a newer save";

} // namespace

SetupPortalStartResult service_setup_portal_start_request()
{
    if (!setup_portal_start_requested()) {
        return SetupPortalStartResult::kNoRequest;
    }
    if (!start_wifi_radio(true)) {
        ESP_LOGW(TAG, "%s", kSetupPortalStartFailedLog);
        return SetupPortalStartResult::kRetryPending;
    }
    app_event_group_clear_bits(kSetupPortalStartBit);
    settings_page_clear();
    network_diag_page_clear();
    info_page_clear();
    notify_ui_task();
    ESP_LOGI(TAG, "%s", kSetupPortalStartedLog);
    return SetupPortalStartResult::kStarted;
}

SetupPortalStopResult service_setup_portal_stop_request()
{
    if (!setup_portal_stop_requested()) {
        return SetupPortalStopResult::kNoRequest;
    }
    stop_wifi_radio(true);
    if (wifi_radio_on_load() || setup_portal_active_load()) {
        ESP_LOGW(TAG, "%s", kSetupPortalStopFailedLog);
        return SetupPortalStopResult::kRetryPending;
    }
    complete_setup_portal_stop_request();
    ESP_LOGI(TAG, "%s", kSetupPortalStoppedLog);
    return SetupPortalStopResult::kStopped;
}

bool publish_setup_portal_result(WifiPortalSaveResult result,
                                 uint32_t expected_generation)
{
    if (wifi_portal_save_snapshot_load().generation != expected_generation) {
        return false;
    }
    if (!prepare_setup_portal_result_delivery()) {
        ESP_LOGW(TAG, "%s", PROVISIONING_RESULT_DELIVERY_DEGRADED_FORMAT);
    }
    return wifi_portal_save_result_store_if_generation(
        expected_generation,
        result,
        nullptr);
}

void complete_provisioning_sync_request(uint32_t expected_generation)
{
    app_event_group_clear_bits(kProvisioningSyncBit);
    const WifiPortalSaveSnapshot current =
        wifi_portal_save_snapshot_load();
    if (current.generation != expected_generation &&
        current.result == WifiPortalSaveResult::kValidating) {
        // A newer save can reuse the same level-triggered bit while the old
        // validation is finishing. Restore it after retiring the old request.
        app_event_group_set_bits(kProvisioningSyncBit);
    }
}

bool wait_for_provisioning_result_feedback(uint32_t expected_generation)
{
    const TickType_t started_at = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(kProvisioningFeedbackWaitMs);
    const TickType_t deadline = started_at + timeout_ticks;
    WifiPortalSaveSnapshot snapshot = {};
    bool seen = false;
    for (;;) {
        snapshot = wifi_portal_save_snapshot_load();
        if (snapshot.generation != expected_generation) {
            break;
        }
        if (!setup_portal_active_load()) {
            break;
        }
        if (snapshot.feedback_seen) {
            seen = true;
            break;
        }
        const TickType_t now = xTaskGetTickCount();
        const TickType_t remaining =
            app_tick_deadline_remaining(now, deadline);
        if (remaining == 0) {
            break;
        }
        if (app_event_group_ready()) {
            const EventBits_t bits = app_event_group_wait_bits(
                kProvisioningFeedbackBit | kProvisioningSyncBit,
                pdFALSE,
                pdFALSE,
                remaining);
            if ((bits & kProvisioningFeedbackBit) != 0) {
                app_event_group_clear_bits(kProvisioningFeedbackBit);
            }
        } else {
            // The application event group is a mandatory boot resource. Keep
            // the old timeout behavior as a defensive fallback if it is lost.
            vTaskDelay(remaining);
            break;
        }
    }
    if (seen) {
        vTaskDelay(pdMS_TO_TICKS(kProvisioningFeedbackDisplayGraceMs));
    }
    const bool still_current =
        wifi_portal_save_snapshot_load().generation == expected_generation;
    const uint32_t elapsed_ms = static_cast<uint32_t>(
        (xTaskGetTickCount() - started_at) * portTICK_PERIOD_MS);
    ESP_LOGI(TAG,
             PROVISIONING_FEEDBACK_WAIT_DONE_FORMAT,
             seen,
             still_current,
             static_cast<unsigned>(elapsed_ms));
    return still_current;
}

void keep_setup_portal_after_provisioning_failure(
    NetworkAwakeLockGuard &awake_lock,
    WifiPortalSaveResult result,
    uint32_t expected_generation)
{
    if (!publish_setup_portal_result(result, expected_generation)) {
        awake_lock.release();
        ESP_LOGI(TAG, "%s", kProvisioningAttemptSupersededLog);
        return;
    }
    // The AP and HTTP server must remain available so the phone can display
    // the stored error and submit corrected credentials. Only release the CPU
    // awake lock owned by this validation attempt.
    awake_lock.release();
    ESP_LOGW(TAG, "%s", kProvisioningValidationFailedKeepPortalLog);
}
