// 定义 OTA UI 状态及其提示保持期的纯判断规则。
#pragma once

#include "app_tick_time.h"

enum OtaUiState {
    kOtaIdle = 0,
    kOtaChecking = 1,
    kOtaAvailable = 2,
    kOtaUpdating = 3,
    kOtaSucceeded = 4,
    kOtaFailed = 5,
    kOtaNoUpdate = 6,
};

enum class OtaWifiFinishPolicy {
    kReleaseAwakeLock,
    kHoldAwakeLockUntilRestart,
};

enum class OtaNetworkBlockReason {
    kNone,
    kOfflineMode,
    kNoWifi,
    kLowBattery,
    kSetupPortal,
};

struct OtaNetworkContinuationState {
    bool offline_mode = false;
    bool wifi_configured = false;
    bool low_battery = false;
    bool setup_portal_start_requested = false;
    bool setup_portal_active = false;
};

constexpr OtaNetworkBlockReason ota_network_block_reason(
    const OtaNetworkContinuationState &state)
{
    if (state.offline_mode) {
        return OtaNetworkBlockReason::kOfflineMode;
    }
    if (!state.wifi_configured) {
        return OtaNetworkBlockReason::kNoWifi;
    }
    if (state.low_battery) {
        return OtaNetworkBlockReason::kLowBattery;
    }
    if (state.setup_portal_start_requested || state.setup_portal_active) {
        return OtaNetworkBlockReason::kSetupPortal;
    }
    return OtaNetworkBlockReason::kNone;
}

constexpr OtaWifiFinishPolicy ota_wifi_finish_policy(bool update_succeeded)
{
    return update_succeeded
               ? OtaWifiFinishPolicy::kHoldAwakeLockUntilRestart
               : OtaWifiFinishPolicy::kReleaseAwakeLock;
}

constexpr bool ota_blocks_background_network_sync(int state)
{
    return state == kOtaChecking || state == kOtaUpdating;
}

constexpr bool ota_background_network_block_changed(int previous_state,
                                                    int current_state)
{
    return ota_blocks_background_network_sync(previous_state) !=
           ota_blocks_background_network_sync(current_state);
}

template <typename Tick>
constexpr bool ota_status_hold_active_for_tick(bool hold_set,
                                               Tick now,
                                               Tick deadline)
{
    return hold_set && app_tick_deadline_pending(now, deadline);
}

template <typename Tick>
constexpr bool ota_flow_active_for_tick(int state,
                                        bool hold_set,
                                        Tick now,
                                        Tick deadline)
{
    const bool hold_active = ota_status_hold_active_for_tick(hold_set,
                                                             now,
                                                             deadline);
    return state == kOtaChecking ||
           (state == kOtaAvailable && hold_active) ||
           state == kOtaUpdating ||
           (state == kOtaSucceeded && hold_active);
}

template <typename Tick>
constexpr bool ota_status_should_reset_to_idle(int state,
                                               bool hold_set,
                                               Tick now,
                                               Tick deadline)
{
    return !ota_flow_active_for_tick(state, hold_set, now, deadline) &&
           state != kOtaIdle &&
           hold_set &&
           app_tick_deadline_reached(now, deadline);
}

static_assert(kOtaIdle == 0 && kOtaChecking == 1 && kOtaAvailable == 2 &&
                  kOtaUpdating == 3 && kOtaSucceeded == 4 &&
                  kOtaFailed == 5 && kOtaNoUpdate == 6,
              "OTA UI state values must remain storage-compatible");
