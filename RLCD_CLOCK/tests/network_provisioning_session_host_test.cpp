// 验证配网热点三态启动、结果反馈等待和失败门户保留事务。
#include "network_provisioning_session_internal.h"

#include "app_event_group.h"
#include "app_metadata.h"
#include "network_task_guards.h"

#include <cassert>

const char *const TAG = "ProvisioningSessionHost";

namespace {

bool s_start_requested = false;
bool s_start_wifi_result = false;
int s_start_wifi_calls = 0;
EventBits_t s_cleared_bits = 0;
EventBits_t s_event_bits = 0;
EventBits_t s_set_bits = 0;
int s_settings_clear_calls = 0;
int s_diag_clear_calls = 0;
int s_info_clear_calls = 0;
int s_notify_calls = 0;
bool s_stop_requested = false;
bool s_stop_succeeds = false;
bool s_wifi_radio_on = false;
int s_stop_wifi_calls = 0;
int s_complete_stop_calls = 0;

bool s_portal_active = false;
bool s_feedback_seen = false;
uint32_t s_save_generation = 1;
uint32_t s_generation_after_wait = 0;
uint32_t s_generation_after_delay = 0;
TickType_t s_ticks = 0;
int s_delay_calls = 0;
TickType_t s_last_delay = 0;
int s_wait_calls = 0;
EventBits_t s_wait_bits = 0;
EventBits_t s_wait_result = 0;
EventBits_t s_wait_result_after_first = 0;
TickType_t s_wait_timeout = 0;
TickType_t s_wait_elapsed_on_first_result = 0;
bool s_event_group_ready = true;

bool s_prepare_result = false;
int s_prepare_calls = 0;
WifiPortalSaveResult s_stored_result = WifiPortalSaveResult::kNone;
int s_store_calls = 0;
bool s_awake_lock_available = true;
int s_awake_acquire_calls = 0;
int s_awake_release_calls = 0;

void reset_start_state()
{
    s_start_requested = false;
    s_start_wifi_result = false;
    s_start_wifi_calls = 0;
    s_cleared_bits = 0;
    s_event_bits = 0;
    s_set_bits = 0;
    s_settings_clear_calls = 0;
    s_diag_clear_calls = 0;
    s_info_clear_calls = 0;
    s_notify_calls = 0;
}

void reset_stop_state()
{
    s_stop_requested = false;
    s_stop_succeeds = false;
    s_wifi_radio_on = false;
    s_portal_active = false;
    s_stop_wifi_calls = 0;
    s_complete_stop_calls = 0;
}

void reset_feedback_state()
{
    s_cleared_bits = 0;
    s_event_bits = 0;
    s_set_bits = 0;
    s_portal_active = false;
    s_feedback_seen = false;
    s_save_generation = 1;
    s_generation_after_wait = 0;
    s_generation_after_delay = 0;
    s_ticks = 0;
    s_delay_calls = 0;
    s_last_delay = 0;
    s_wait_calls = 0;
    s_wait_bits = 0;
    s_wait_result = 0;
    s_wait_result_after_first = 0;
    s_wait_timeout = 0;
    s_wait_elapsed_on_first_result = 0;
    s_event_group_ready = true;
}

} // namespace

bool setup_portal_start_requested()
{
    return s_start_requested;
}

bool start_wifi_radio(bool force_setup_portal)
{
    assert(force_setup_portal);
    ++s_start_wifi_calls;
    return s_start_wifi_result;
}

bool setup_portal_stop_requested()
{
    return s_stop_requested;
}

void stop_wifi_radio(bool force_setup_portal)
{
    assert(force_setup_portal);
    ++s_stop_wifi_calls;
    if (s_stop_succeeds) {
        s_wifi_radio_on = false;
        s_portal_active = false;
    }
}

bool wifi_radio_on_load()
{
    return s_wifi_radio_on;
}

void complete_setup_portal_stop_request()
{
    s_stop_requested = false;
    ++s_complete_stop_calls;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    s_cleared_bits |= bits;
    const EventBits_t previous = s_event_bits;
    s_event_bits &= ~bits;
    return previous;
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    s_set_bits |= bits;
    s_event_bits |= bits;
    return s_event_bits;
}

void settings_page_clear()
{
    ++s_settings_clear_calls;
}

void network_diag_page_clear()
{
    ++s_diag_clear_calls;
}

void info_page_clear()
{
    ++s_info_clear_calls;
}

void notify_ui_task()
{
    ++s_notify_calls;
}

bool setup_portal_active_load()
{
    return s_portal_active;
}

bool wifi_portal_save_feedback_seen_load()
{
    return s_feedback_seen;
}

TickType_t xTaskGetTickCount()
{
    return s_ticks;
}

bool app_event_group_ready()
{
    return s_event_group_ready;
}

EventBits_t app_event_group_wait_bits(EventBits_t bits,
                                      BaseType_t clear_on_exit,
                                      BaseType_t wait_for_all,
                                      TickType_t timeout)
{
    assert(clear_on_exit == pdFALSE);
    assert(wait_for_all == pdFALSE);
    ++s_wait_calls;
    s_wait_bits = bits;
    s_wait_timeout = timeout;
    const EventBits_t result = s_wait_calls == 1
                                   ? s_wait_result
                                   : s_wait_result_after_first;
    if (result == 0) {
        s_ticks += timeout;
    } else if (s_wait_calls == 1) {
        s_ticks += s_wait_elapsed_on_first_result;
    }
    if (s_generation_after_wait != 0) {
        s_save_generation = s_generation_after_wait;
    }
    return result;
}

void vTaskDelay(TickType_t ticks)
{
    s_last_delay = ticks;
    s_ticks += ticks;
    ++s_delay_calls;
    if (s_generation_after_delay != 0) {
        s_save_generation = s_generation_after_delay;
    }
}

bool prepare_setup_portal_result_delivery()
{
    ++s_prepare_calls;
    return s_prepare_result;
}

WifiPortalSaveSnapshot wifi_portal_save_snapshot_load()
{
    return {s_stored_result, s_feedback_seen, s_save_generation};
}

bool wifi_portal_save_result_store_if_generation(
    uint32_t generation,
    WifiPortalSaveResult result,
    WifiPortalSaveSnapshot *updated)
{
    if (generation != s_save_generation) {
        return false;
    }
    s_stored_result = result;
    s_feedback_seen = false;
    ++s_store_calls;
    if (updated) {
        *updated = {result, false, s_save_generation};
    }
    return true;
}

bool acquire_network_awake_lock()
{
    ++s_awake_acquire_calls;
    return s_awake_lock_available;
}

void release_network_awake_lock()
{
    ++s_awake_release_calls;
}

int main()
{
    reset_start_state();
    assert(service_setup_portal_start_request() == SetupPortalStartResult::kNoRequest);
    assert(s_start_wifi_calls == 0);

    reset_start_state();
    s_start_requested = true;
    assert(service_setup_portal_start_request() == SetupPortalStartResult::kRetryPending);
    assert(s_start_wifi_calls == 1);
    assert(s_cleared_bits == 0);
    assert(s_notify_calls == 0);

    reset_start_state();
    s_start_requested = true;
    s_start_wifi_result = true;
    assert(service_setup_portal_start_request() == SetupPortalStartResult::kStarted);
    assert(s_start_wifi_calls == 1);
    assert(s_cleared_bits == kSetupPortalStartBit);
    assert(s_settings_clear_calls == 1);
    assert(s_diag_clear_calls == 1);
    assert(s_info_clear_calls == 1);
    assert(s_notify_calls == 1);

    reset_stop_state();
    assert(service_setup_portal_stop_request() == SetupPortalStopResult::kNoRequest);
    assert(s_stop_wifi_calls == 0);
    assert(s_complete_stop_calls == 0);

    reset_stop_state();
    s_stop_requested = true;
    s_wifi_radio_on = true;
    s_portal_active = true;
    assert(service_setup_portal_stop_request() == SetupPortalStopResult::kRetryPending);
    assert(s_stop_wifi_calls == 1);
    assert(s_stop_requested);
    assert(s_complete_stop_calls == 0);

    reset_stop_state();
    s_stop_requested = true;
    s_stop_succeeds = true;
    s_wifi_radio_on = true;
    s_portal_active = true;
    assert(service_setup_portal_stop_request() == SetupPortalStopResult::kStopped);
    assert(s_stop_wifi_calls == 1);
    assert(!s_stop_requested);
    assert(s_complete_stop_calls == 1);

    reset_feedback_state();
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_delay_calls == 0);
    assert(s_ticks == 0);

    reset_feedback_state();
    s_portal_active = true;
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 1);
    assert(s_wait_bits ==
           (kProvisioningFeedbackBit | kProvisioningSyncBit));
    assert(s_wait_timeout == pdMS_TO_TICKS(30000));
    assert(s_delay_calls == 0);
    assert(s_ticks == pdMS_TO_TICKS(30000));

    reset_feedback_state();
    s_portal_active = true;
    s_wait_result = kProvisioningFeedbackBit;
    s_wait_elapsed_on_first_result = pdMS_TO_TICKS(29950);
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 2);
    assert(s_wait_timeout == pdMS_TO_TICKS(50));
    assert(s_ticks == pdMS_TO_TICKS(30000));

    reset_feedback_state();
    s_portal_active = true;
    s_wait_result = kProvisioningFeedbackBit;
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 2);
    assert(s_delay_calls == 0);
    assert(s_ticks == pdMS_TO_TICKS(30000));

    reset_feedback_state();
    s_portal_active = true;
    s_feedback_seen = true;
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 0);
    assert(s_delay_calls == 1);
    assert(s_ticks == pdMS_TO_TICKS(750));
    assert(s_last_delay == pdMS_TO_TICKS(750));

    reset_feedback_state();
    s_portal_active = true;
    s_event_group_ready = false;
    assert(wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 0);
    assert(s_delay_calls == 1);
    assert(s_ticks == pdMS_TO_TICKS(30000));
    assert(s_last_delay == pdMS_TO_TICKS(30000));

    reset_feedback_state();
    s_prepare_result = false;
    s_prepare_calls = 0;
    s_stored_result = WifiPortalSaveResult::kNone;
    s_store_calls = 0;
    assert(publish_setup_portal_result(
        WifiPortalSaveResult::kWeatherApiFailed,
        1));
    assert(s_prepare_calls == 1);
    assert(s_delay_calls == 0);
    assert(s_store_calls == 1);
    assert(s_stored_result == WifiPortalSaveResult::kWeatherApiFailed);

    reset_feedback_state();
    s_prepare_result = true;
    s_prepare_calls = 0;
    s_store_calls = 0;
    s_save_generation = 2;
    assert(!publish_setup_portal_result(
        WifiPortalSaveResult::kSuccess,
        1));
    assert(s_prepare_calls == 0);
    assert(s_store_calls == 0);

    reset_feedback_state();
    s_event_bits = kProvisioningSyncBit;
    complete_provisioning_sync_request(1);
    assert((s_event_bits & kProvisioningSyncBit) == 0);
    assert((s_set_bits & kProvisioningSyncBit) == 0);

    reset_feedback_state();
    s_event_bits = kProvisioningSyncBit;
    s_save_generation = 2;
    s_stored_result = WifiPortalSaveResult::kValidating;
    complete_provisioning_sync_request(1);
    assert((s_event_bits & kProvisioningSyncBit) != 0);
    assert((s_set_bits & kProvisioningSyncBit) != 0);

    reset_feedback_state();
    s_event_bits = kProvisioningSyncBit;
    s_save_generation = 2;
    s_stored_result = WifiPortalSaveResult::kInvalidInput;
    complete_provisioning_sync_request(1);
    assert((s_event_bits & kProvisioningSyncBit) == 0);
    assert((s_set_bits & kProvisioningSyncBit) == 0);

    reset_feedback_state();
    s_portal_active = true;
    s_wait_result = kProvisioningFeedbackBit;
    s_generation_after_wait = 2;
    assert(!wait_for_provisioning_result_feedback(1));
    assert(s_wait_calls == 1);
    assert(s_delay_calls == 0);
    assert(s_ticks == 0);

    reset_feedback_state();
    s_portal_active = true;
    s_feedback_seen = true;
    s_generation_after_delay = 2;
    assert(!wait_for_provisioning_result_feedback(1));
    assert(s_delay_calls == 1);

    reset_feedback_state();
    s_prepare_result = true;
    s_prepare_calls = 0;
    s_stored_result = WifiPortalSaveResult::kNone;
    s_store_calls = 0;
    s_awake_acquire_calls = 0;
    s_awake_release_calls = 0;
    {
        NetworkAwakeLockGuard awake_lock;
        assert(awake_lock.locked());
        keep_setup_portal_after_provisioning_failure(
            awake_lock,
            WifiPortalSaveResult::kWeatherApiFailed,
            1);
        assert(!awake_lock.locked());
    }
    assert(s_prepare_calls == 1);
    assert(s_delay_calls == 0);
    assert(s_store_calls == 1);
    assert(s_stored_result == WifiPortalSaveResult::kWeatherApiFailed);
    assert(s_awake_acquire_calls == 1);
    assert(s_awake_release_calls == 1);

    s_awake_lock_available = false;
    s_awake_acquire_calls = 0;
    s_awake_release_calls = 0;
    {
        NetworkAwakeLockGuard awake_lock;
        assert(!awake_lock.locked());
        keep_setup_portal_after_provisioning_failure(
            awake_lock,
            WifiPortalSaveResult::kWifiConnectionFailed,
            1);
    }
    assert(s_awake_acquire_calls == 1);
    assert(s_awake_release_calls == 0);

    return 0;
}
