// 验证 OTA 活跃状态、提示到期和 Tick 回绕边界。
#include "ota_flow_policy.h"
#include "ota_network_session_internal.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    using Tick = uint32_t;
    constexpr Tick kNow = 100;
    constexpr Tick kPendingDeadline = 120;
    constexpr Tick kReachedDeadline = 100;

    assert(!ota_blocks_background_network_sync(kOtaIdle));
    assert(ota_blocks_background_network_sync(kOtaChecking));
    assert(!ota_blocks_background_network_sync(kOtaAvailable));
    assert(ota_blocks_background_network_sync(kOtaUpdating));
    assert(!ota_blocks_background_network_sync(kOtaSucceeded));
    assert(!ota_background_network_block_changed(kOtaChecking, kOtaUpdating));
    assert(ota_background_network_block_changed(kOtaIdle, kOtaChecking));
    assert(ota_background_network_block_changed(kOtaAvailable, kOtaUpdating));
    assert(ota_background_network_block_changed(kOtaUpdating, kOtaFailed));
    assert(!ota_background_network_block_changed(kOtaFailed, kOtaIdle));
    assert(ota_wifi_finish_policy(false) ==
           OtaWifiFinishPolicy::kReleaseAwakeLock);
    assert(ota_wifi_finish_policy(true) ==
           OtaWifiFinishPolicy::kHoldAwakeLockUntilRestart);

    OtaNetworkContinuationState network_state = {};
    network_state.wifi_configured = true;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kNone);
    network_state.offline_mode = true;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kOfflineMode);
    network_state = {};
    network_state.wifi_configured = false;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kNoWifi);
    network_state.wifi_configured = true;
    network_state.low_battery = true;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kLowBattery);
    network_state.low_battery = false;
    network_state.setup_portal_start_requested = true;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kSetupPortal);
    network_state.setup_portal_start_requested = false;
    network_state.setup_portal_active = true;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kSetupPortal);
    network_state.setup_portal_active = false;
    assert(ota_network_block_reason(network_state) ==
           OtaNetworkBlockReason::kNone);
    assert(ota_network_session_start_result_for_block(
               OtaNetworkBlockReason::kNone) ==
           OtaNetworkSessionStartResult::kReady);
    assert(ota_network_session_start_result_for_block(
               OtaNetworkBlockReason::kOfflineMode) ==
           OtaNetworkSessionStartResult::kOfflineMode);
    assert(ota_network_session_start_result_for_block(
               OtaNetworkBlockReason::kNoWifi) ==
           OtaNetworkSessionStartResult::kNoWifi);
    assert(ota_network_session_start_result_for_block(
               OtaNetworkBlockReason::kLowBattery) ==
           OtaNetworkSessionStartResult::kLowBattery);
    assert(ota_network_session_start_result_for_block(
               OtaNetworkBlockReason::kSetupPortal) ==
           OtaNetworkSessionStartResult::kSetupPortal);

    assert(ota_status_hold_active_for_tick(true, kNow, kPendingDeadline));
    assert(!ota_status_hold_active_for_tick(false, kNow, kPendingDeadline));
    assert(!ota_status_hold_active_for_tick(true, kNow, kReachedDeadline));

    assert(!ota_flow_active_for_tick(kOtaIdle, false, kNow, kPendingDeadline));
    assert(ota_flow_active_for_tick(kOtaChecking, false, kNow, kReachedDeadline));
    assert(ota_flow_active_for_tick(kOtaAvailable, true, kNow, kPendingDeadline));
    assert(!ota_flow_active_for_tick(kOtaAvailable, true, kNow, kReachedDeadline));
    assert(ota_flow_active_for_tick(kOtaUpdating, false, kNow, kReachedDeadline));
    assert(ota_flow_active_for_tick(kOtaSucceeded, true, kNow, kPendingDeadline));
    assert(!ota_flow_active_for_tick(kOtaSucceeded, true, kNow, kReachedDeadline));
    assert(!ota_flow_active_for_tick(kOtaFailed, true, kNow, kPendingDeadline));
    assert(!ota_flow_active_for_tick(kOtaNoUpdate, true, kNow, kPendingDeadline));

    assert(!ota_status_should_reset_to_idle(kOtaIdle, true, kNow, kReachedDeadline));
    assert(!ota_status_should_reset_to_idle(kOtaChecking, true, kNow, kReachedDeadline));
    assert(!ota_status_should_reset_to_idle(kOtaUpdating, true, kNow, kReachedDeadline));
    assert(!ota_status_should_reset_to_idle(kOtaFailed, false, kNow, kReachedDeadline));
    assert(!ota_status_should_reset_to_idle(kOtaAvailable, true, kNow, kPendingDeadline));
    assert(ota_status_should_reset_to_idle(kOtaAvailable, true, kNow, kReachedDeadline));
    assert(ota_status_should_reset_to_idle(kOtaSucceeded, true, kNow, kReachedDeadline));
    assert(ota_status_should_reset_to_idle(kOtaFailed, true, kNow, kReachedDeadline));
    assert(ota_status_should_reset_to_idle(kOtaNoUpdate, true, kNow, kReachedDeadline));

    constexpr Tick kWrapNow = UINT32_MAX - 5;
    constexpr Tick kWrapDeadline = 4;
    assert(ota_flow_active_for_tick(kOtaAvailable, true, kWrapNow, kWrapDeadline));
    assert(!ota_status_should_reset_to_idle(kOtaAvailable,
                                            true,
                                            kWrapNow,
                                            kWrapDeadline));
    assert(!ota_flow_active_for_tick(kOtaAvailable, true, 4U, kWrapDeadline));
    assert(ota_status_should_reset_to_idle(kOtaAvailable, true, 4U, kWrapDeadline));
    return 0;
}
