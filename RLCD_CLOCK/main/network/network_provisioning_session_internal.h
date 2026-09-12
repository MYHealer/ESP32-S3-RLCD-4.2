// 声明仅供联网调度内部使用的配网热点启停与结果交接事务。
#pragma once

#include "wifi_portal_state.h"

class NetworkAwakeLockGuard;

enum class SetupPortalStartResult {
    kNoRequest,
    kStarted,
    kRetryPending,
};

enum class SetupPortalStopResult {
    kNoRequest,
    kStopped,
    kRetryPending,
};

SetupPortalStartResult service_setup_portal_start_request();
SetupPortalStopResult service_setup_portal_stop_request();
bool publish_setup_portal_result(WifiPortalSaveResult result,
                                 uint32_t expected_generation);
void complete_provisioning_sync_request(uint32_t expected_generation);
bool wait_for_provisioning_result_feedback(uint32_t expected_generation);
void keep_setup_portal_after_provisioning_failure(
    NetworkAwakeLockGuard &awake_lock,
    WifiPortalSaveResult result,
    uint32_t expected_generation);
