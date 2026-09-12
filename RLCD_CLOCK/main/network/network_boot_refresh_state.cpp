// 维护启动天气、每日文字待办、截止时间和 HTTPS 资源退避状态。
#include "network_boot_refresh_state.h"

#include "app_constexpr.h"
#include "network_sync_schedule.h"

namespace {
constexpr uint8_t kBootRefreshFailureRetryLimit = 2;
constexpr time_t kBootRefreshFailureRetryDelaySeconds = 2 * 60;
static_assert(kBootRefreshFailureRetryLimit > 0,
              "boot refresh must retain a bounded retry opportunity");
static_assert(kBootRefreshFailureRetryDelaySeconds > 0,
              "boot refresh retry delay must be positive");

void normalize_network_boot_refresh_state(NetworkBootRefreshState *state)
{
    if (!state->weather_due) {
        state->weather_due_at = 0;
        state->weather_resource_deferrals = 0;
        state->weather_failures = 0;
    }
    if (!state->saying_due) {
        state->saying_due_at = 0;
        state->saying_failures = 0;
    }
    if (!state->weather_due && !state->saying_due) {
        state->https_memory_deferrals = 0;
    }
}

NetworkBootWeatherAttemptUpdate finish_failed_boot_refresh_attempt(
    bool *due,
    time_t *due_at,
    uint8_t *failures,
    time_t now)
{
    NetworkBootWeatherAttemptUpdate update;
    if (!due || !due_at || !failures) {
        return update;
    }
    *failures = saturating_increment_u8(*failures);
    update.failure_count = *failures;
    if (*failures <= kBootRefreshFailureRetryLimit) {
        *due = true;
        *due_at = now + kBootRefreshFailureRetryDelaySeconds;
        update.delay_seconds = kBootRefreshFailureRetryDelaySeconds;
        update.retry_scheduled = true;
        return update;
    }
    *due = false;
    update.retry_exhausted = true;
    return update;
}
} // namespace

NetworkBootRefreshState initialize_network_boot_refresh_state(
    bool weather_due,
    bool saying_due,
    time_t weather_due_at,
    time_t saying_due_at)
{
    NetworkBootRefreshState state;
    schedule_network_boot_refreshes(
        &state,
        weather_due,
        saying_due,
        weather_due_at,
        saying_due_at);
    return state;
}

void schedule_network_boot_refreshes(
    NetworkBootRefreshState *state,
    bool weather_due,
    bool saying_due,
    time_t weather_due_at,
    time_t saying_due_at)
{
    if (!state) {
        return;
    }
    state->weather_due = weather_due;
    state->saying_due = saying_due;
    state->weather_due_at = weather_due ? weather_due_at : 0;
    state->saying_due_at = saying_due ? saying_due_at : 0;
    state->https_memory_deferrals = 0;
    state->weather_resource_deferrals = 0;
    state->weather_failures = 0;
    state->saying_failures = 0;
}

void clear_network_boot_refreshes(NetworkBootRefreshState *state)
{
    if (!state) {
        return;
    }
    *state = {};
}

void reconcile_network_boot_refresh_state(
    NetworkBootRefreshState *state,
    bool weather_page_enabled,
    bool saying_page_enabled,
    bool weather_cache_current,
    bool saying_cache_current,
    bool blocked)
{
    if (!state) {
        return;
    }
    if (blocked) {
        clear_network_boot_refreshes(state);
        return;
    }
    if (state->weather_due &&
        (!weather_page_enabled || weather_cache_current)) {
        state->weather_due = false;
    }
    if (state->saying_due &&
        (!saying_page_enabled || saying_cache_current)) {
        state->saying_due = false;
    }
    normalize_network_boot_refresh_state(state);
}

NetworkBootHttpsMemoryDeferralUpdate defer_network_boot_refreshes_for_memory(
    NetworkBootRefreshState *state,
    NetworkSyncSchedule *schedule,
    NetworkBootHttpsDeferralInput input,
    bool memory_allowed)
{
    NetworkBootHttpsMemoryDeferralUpdate update;
    if (!state || !schedule) {
        return update;
    }
    input.memory_allowed = memory_allowed;
    if (!network_automatic_boot_https_pending(*schedule, input)) {
        normalize_network_boot_refresh_state(state);
        return update;
    }
    if (memory_allowed) {
        state->https_memory_deferrals = 0;
        return update;
    }
    state->https_memory_deferrals = saturating_increment_u8(
        state->https_memory_deferrals);
    input.retry_delay_seconds =
        network_boot_https_memory_retry_delay_seconds(
            state->https_memory_deferrals);
    const NetworkBootHttpsDeferralResult result =
        calculate_network_boot_https_deferral(*schedule, input);
    if (!result.deferred) {
        state->https_memory_deferrals = 0;
        return update;
    }

    *schedule = result.schedule;
    if (result.weather_deferred) {
        state->weather_due_at = result.retry_at;
    }
    if (result.saying_deferred) {
        state->saying_due_at = result.retry_at;
    }
    update.delay_seconds = input.retry_delay_seconds;
    update.deferral_count = state->https_memory_deferrals;
    update.deferred = true;
    update.weather_deferred = result.weather_deferred;
    update.saying_deferred = result.saying_deferred;
    return update;
}

NetworkBootWeatherAttemptUpdate finish_network_boot_weather_attempt(
    NetworkBootRefreshState *state,
    bool weather_ready,
    bool succeeded,
    bool resource_deferred,
    time_t now)
{
    NetworkBootWeatherAttemptUpdate update;
    if (!state || !weather_ready) {
        return update;
    }
    if (resource_deferred) {
        state->weather_due = true;
        state->weather_resource_deferrals = saturating_increment_u8(
            state->weather_resource_deferrals);
        update.delay_seconds =
            network_boot_https_memory_retry_delay_seconds(
                state->weather_resource_deferrals);
        update.deferral_count = state->weather_resource_deferrals;
        update.retry_scheduled = true;
        state->weather_due_at = now + update.delay_seconds;
    } else if (succeeded) {
        state->weather_due = false;
    } else {
        update = finish_failed_boot_refresh_attempt(
            &state->weather_due,
            &state->weather_due_at,
            &state->weather_failures,
            now);
    }
    normalize_network_boot_refresh_state(state);
    return update;
}

NetworkBootSayingAttemptUpdate finish_network_boot_saying_attempt(
    NetworkBootRefreshState *state,
    bool saying_ready,
    bool succeeded,
    time_t now)
{
    NetworkBootSayingAttemptUpdate update;
    if (!state || !saying_ready) {
        return update;
    }
    if (succeeded) {
        state->saying_due = false;
    } else {
        update = finish_failed_boot_refresh_attempt(
            &state->saying_due,
            &state->saying_due_at,
            &state->saying_failures,
            now);
    }
    normalize_network_boot_refresh_state(state);
    return update;
}

bool stagger_network_boot_saying(
    NetworkBootRefreshState *state,
    bool should_stagger,
    time_t now,
    time_t gap_seconds)
{
    if (!state || !should_stagger || !state->saying_due) {
        return false;
    }
    state->saying_due_at = now + gap_seconds;
    return true;
}
