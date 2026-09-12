// 维护联网任务 NTP 启动待办、每日零点截止时间和失败退避状态。
#include "network_ntp_schedule_state.h"

#include "app_constexpr.h"
#include "network_sync_schedule.h"
#include "sensor_time.h"

namespace {
void clear_retry(NetworkNtpScheduleState *state)
{
    state->next_retry_at = 0;
    state->retry_failures = 0;
}
} // namespace

NetworkNtpScheduleState initialize_network_ntp_schedule(bool already_synced,
                                                        time_t now)
{
    NetworkNtpScheduleState state;
    state.boot_due = !already_synced;
    state.next_daily_at = state.boot_due
                              ? 0
                              : next_local_midnight_time(now);
    return state;
}

NetworkNtpRetryUpdate finish_network_ntp_attempt(
    NetworkNtpScheduleState *state,
    bool succeeded,
    bool retry_required,
    bool time_plausible,
    time_t now)
{
    NetworkNtpRetryUpdate update;
    if (!state) {
        return update;
    }
    if (succeeded) {
        state->boot_due = false;
        state->daily_pending = false;
        clear_retry(state);
        state->next_daily_at = next_local_midnight_time(now);
        return update;
    }
    if (!retry_required) {
        clear_retry(state);
        return update;
    }
    state->retry_failures = saturating_increment_u8(
        state->retry_failures);
    update.delay_seconds = network_ntp_retry_delay_seconds(
        time_plausible,
        state->retry_failures);
    update.scheduled = true;
    state->next_retry_at = now + update.delay_seconds;
    return update;
}

void refresh_network_ntp_daily_due(NetworkNtpScheduleState *state,
                                   time_t now,
                                   bool time_plausible)
{
    if (!state) {
        return;
    }
    if (!state->daily_pending &&
        state->next_daily_at > 0 &&
        now >= state->next_daily_at) {
        state->daily_pending = true;
        state->next_daily_at = 0;
    }
    if (!state->daily_pending &&
        state->next_daily_at == 0 &&
        !state->boot_due &&
        time_plausible) {
        state->next_daily_at = next_local_midnight_time(now);
    }
}

void schedule_network_ntp_after_provisioning(
    NetworkNtpScheduleState *state)
{
    if (!state) {
        return;
    }
    state->boot_due = true;
    state->daily_pending = false;
    state->next_daily_at = 0;
    clear_retry(state);
}
