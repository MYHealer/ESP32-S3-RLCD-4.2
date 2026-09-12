// 实现 OTA 专用 Wi-Fi 连接等待、运行态复核和网络资源收尾。
#include "ota_network_session_internal.h"

#include "app_event_group.h"
#include "app_metadata.h"
#include "app_tick_time.h"
#include "battery_policy.h"
#include "battery_runtime_state.h"
#include "network_credentials_state.h"
#include "offline_mode_state.h"
#include "power_services.h"
#include "setup_portal_control.h"
#include "wifi_portal_state.h"
#include "wifi_radio_services.h"

#include <esp_log.h>
#include <esp_timer.h>

namespace {

constexpr int64_t kUsPerMs = 1000;

enum class OtaWifiConnectionWaitResult {
    kConnected,
    kRuntimeBlocked,
    kTimedOut,
};

OtaNetworkBlockReason ota_network_block_reason_load()
{
    const BatteryRuntimeStatusSnapshot battery =
        battery_runtime_status_load();

    OtaNetworkContinuationState state = {};
    state.offline_mode = offline_mode_enabled_load();
    state.wifi_configured = network_wifi_credentials_configured();
    state.low_battery =
        battery.low_battery_mode ||
        (battery.percent >= 0 && battery.percent < kLowBatteryEnterPercent);
    state.setup_portal_start_requested = setup_portal_start_requested();
    state.setup_portal_active = setup_portal_active_load();
    return ota_network_block_reason(state);
}

OtaWifiConnectionWaitResult wait_for_ota_wifi_connection(
    uint32_t timeout_ms,
    OtaNetworkBlockReason *block_reason)
{
    constexpr EventBits_t kOtaConnectionWakeBits =
        kWifiConnectedBit | kNetworkStateChangedBit;
    const int64_t deadline_us =
        esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * kUsPerMs;

    for (;;) {
        const OtaNetworkBlockReason reason = ota_network_block_reason_load();
        if (reason != OtaNetworkBlockReason::kNone) {
            if (block_reason) {
                *block_reason = reason;
            }
            return OtaWifiConnectionWaitResult::kRuntimeBlocked;
        }
        if ((app_event_group_get_bits() & kWifiConnectedBit) != 0) {
            return OtaWifiConnectionWaitResult::kConnected;
        }

        const int64_t remaining_us = deadline_us - esp_timer_get_time();
        if (remaining_us <= 0) {
            return OtaWifiConnectionWaitResult::kTimedOut;
        }
        const uint32_t remaining_ms =
            static_cast<uint32_t>((remaining_us + kUsPerMs - 1) / kUsPerMs);
        const TickType_t remaining_ticks =
            app_tick_nonzero_delay(pdMS_TO_TICKS(remaining_ms));
        const EventBits_t bits = app_event_group_wait_bits(
            kOtaConnectionWakeBits,
            pdFALSE,
            pdFALSE,
            remaining_ticks);
        if ((bits & kNetworkStateChangedBit) != 0) {
            // OTA status changes publish the same edge. Ignore that stale edge
            // when the live network policy still permits this OTA session.
            app_event_group_clear_bits(kNetworkStateChangedBit);
        }

        const OtaNetworkBlockReason updated_reason =
            ota_network_block_reason_load();
        if (updated_reason != OtaNetworkBlockReason::kNone) {
            if (block_reason) {
                *block_reason = updated_reason;
            }
            return OtaWifiConnectionWaitResult::kRuntimeBlocked;
        }
        if ((bits & kWifiConnectedBit) != 0) {
            return OtaWifiConnectionWaitResult::kConnected;
        }
        if (bits == 0) {
            return OtaWifiConnectionWaitResult::kTimedOut;
        }
    }
}

} // namespace

OtaNetworkSessionStartResult ota_network_session_start(uint32_t timeout_ms)
{
    const OtaNetworkBlockReason initial_block_reason =
        ota_network_block_reason_load();
    if (initial_block_reason != OtaNetworkBlockReason::kNone) {
        return ota_network_session_start_result_for_block(
            initial_block_reason);
    }
    if (!acquire_network_awake_lock()) {
        ESP_LOGW(TAG, "OTA network PM lock unavailable");
        return OtaNetworkSessionStartResult::kAwakeLockUnavailable;
    }
    if (!start_wifi_radio(false)) {
        release_network_awake_lock();
        // A failed radio reconfiguration may leave the driver active. Keep a
        // deferred close request after releasing this session's PM ownership.
        request_wifi_radio_stop_if_running();
        return OtaNetworkSessionStartResult::kRadioStartFailed;
    }

    OtaNetworkBlockReason wait_block_reason = OtaNetworkBlockReason::kNone;
    const OtaWifiConnectionWaitResult wait_result =
        wait_for_ota_wifi_connection(timeout_ms, &wait_block_reason);
    if (wait_result != OtaWifiConnectionWaitResult::kConnected) {
        stop_wifi_radio(true);
        release_network_awake_lock();
        request_wifi_radio_stop_if_running();
        if (wait_result == OtaWifiConnectionWaitResult::kRuntimeBlocked) {
            return ota_network_session_start_result_for_block(
                wait_block_reason);
        }
        return OtaNetworkSessionStartResult::kConnectionTimedOut;
    }
    return OtaNetworkSessionStartResult::kReady;
}

void ota_network_session_finish(OtaWifiFinishPolicy policy)
{
    stop_wifi_radio(true);
    if (policy == OtaWifiFinishPolicy::kReleaseAwakeLock) {
        release_network_awake_lock();
        request_wifi_radio_stop_if_running();
    }
}
