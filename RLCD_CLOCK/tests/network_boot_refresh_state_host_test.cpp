// 验证启动天气与每日文字待办、截止时间和资源退避状态转换。
#include "network_boot_refresh_state.h"

#include "network_sync_schedule.h"

#include <assert.h>

namespace {
constexpr time_t kNow = 1000;
constexpr time_t kWeatherDueAt = 1010;
constexpr time_t kSayingDueAt = 1025;
}

int main()
{
    NetworkBootRefreshState state = initialize_network_boot_refresh_state(
        true,
        true,
        kWeatherDueAt,
        kSayingDueAt);
    assert(state.weather_due);
    assert(state.saying_due);
    assert(state.weather_due_at == kWeatherDueAt);
    assert(state.saying_due_at == kSayingDueAt);
    assert(state.https_memory_deferrals == 0);
    assert(state.weather_resource_deferrals == 0);
    assert(state.weather_failures == 0);
    assert(state.saying_failures == 0);

    reconcile_network_boot_refresh_state(
        &state,
        false,
        true,
        false,
        false,
        false);
    assert(!state.weather_due);
    assert(state.weather_due_at == 0);
    assert(state.weather_resource_deferrals == 0);
    assert(state.saying_due);
    assert(state.saying_due_at == kSayingDueAt);

    state = initialize_network_boot_refresh_state(
        true,
        true,
        kWeatherDueAt,
        kSayingDueAt);
    reconcile_network_boot_refresh_state(
        &state,
        true,
        true,
        true,
        true,
        false);
    assert(!state.weather_due);
    assert(!state.saying_due);
    assert(state.weather_due_at == 0);
    assert(state.saying_due_at == 0);
    assert(state.https_memory_deferrals == 0);

    schedule_network_boot_refreshes(
        &state,
        true,
        true,
        kWeatherDueAt,
        kSayingDueAt);
    state.https_memory_deferrals = 3;
    state.weather_resource_deferrals = 2;
    reconcile_network_boot_refresh_state(
        &state,
        true,
        true,
        false,
        false,
        true);
    assert(!state.weather_due);
    assert(!state.saying_due);
    assert(state.weather_due_at == 0);
    assert(state.saying_due_at == 0);
    assert(state.https_memory_deferrals == 0);
    assert(state.weather_resource_deferrals == 0);

    schedule_network_boot_refreshes(
        &state,
        true,
        true,
        kWeatherDueAt,
        kSayingDueAt);
    finish_network_boot_weather_attempt(&state, true, true, false, kNow);
    assert(!state.weather_due);
    assert(state.weather_due_at == 0);
    assert(state.saying_due);
    finish_network_boot_saying_attempt(&state, true, true, kNow);
    assert(!state.saying_due);
    assert(state.saying_due_at == 0);
    assert(state.https_memory_deferrals == 0);

    schedule_network_boot_refreshes(
        &state,
        true,
        true,
        kNow,
        kNow);
    NetworkSyncSchedule memory_schedule = {};
    memory_schedule.boot_weather_ready = true;
    memory_schedule.boot_saying_ready = true;
    memory_schedule.weather_due = true;
    memory_schedule.saying_due = true;
    NetworkBootHttpsDeferralInput memory_input = {};
    memory_input.now = kNow;
    NetworkBootHttpsMemoryDeferralUpdate memory_update =
        defer_network_boot_refreshes_for_memory(
            &state,
            &memory_schedule,
            memory_input,
            false);
    assert(memory_update.deferred);
    assert(memory_update.weather_deferred);
    assert(memory_update.saying_deferred);
    assert(memory_update.delay_seconds == 10);
    assert(memory_update.deferral_count == 1);
    assert(state.weather_due_at == kNow + 10);
    assert(state.saying_due_at == kNow + 10);
    assert(!memory_schedule.weather_due);
    assert(!memory_schedule.saying_due);

    memory_schedule.boot_weather_ready = true;
    memory_schedule.boot_saying_ready = false;
    memory_schedule.weather_due = true;
    memory_input.now = kNow + 10;
    memory_update = defer_network_boot_refreshes_for_memory(
        &state,
        &memory_schedule,
        memory_input,
        false);
    assert(memory_update.deferred);
    assert(memory_update.delay_seconds == 20);
    assert(memory_update.deferral_count == 2);
    assert(state.weather_due_at == kNow + 30);

    memory_schedule.boot_weather_ready = true;
    memory_schedule.weather_due = true;
    memory_update = defer_network_boot_refreshes_for_memory(
        &state,
        &memory_schedule,
        memory_input,
        true);
    assert(!memory_update.deferred);
    assert(state.https_memory_deferrals == 0);
    assert(memory_schedule.weather_due);

    NetworkBootWeatherAttemptUpdate weather_update =
        finish_network_boot_weather_attempt(
            &state,
            true,
            false,
            true,
            kNow);
    assert(weather_update.retry_scheduled);
    assert(weather_update.delay_seconds == 10);
    assert(weather_update.deferral_count == 1);
    assert(state.weather_due);
    assert(state.weather_due_at == kNow + 10);

    weather_update = finish_network_boot_weather_attempt(
        &state,
        true,
        true,
        false,
        kNow + 10);
    assert(!weather_update.retry_scheduled);
    assert(!state.weather_due);
    assert(state.weather_due_at == 0);
    assert(state.weather_resource_deferrals == 0);

    schedule_network_boot_refreshes(
        &state,
        true,
        false,
        kNow,
        0);
    weather_update = finish_network_boot_weather_attempt(
        &state,
        true,
        false,
        false,
        kNow);
    assert(weather_update.retry_scheduled);
    assert(!weather_update.retry_exhausted);
    assert(weather_update.delay_seconds == 120);
    assert(weather_update.failure_count == 1);
    assert(state.weather_due);
    assert(state.weather_due_at == kNow + 120);
    assert(state.weather_failures == 1);

    weather_update = finish_network_boot_weather_attempt(
        &state,
        true,
        false,
        false,
        kNow + 120);
    assert(weather_update.retry_scheduled);
    assert(!weather_update.retry_exhausted);
    assert(weather_update.delay_seconds == 120);
    assert(weather_update.failure_count == 2);
    assert(state.weather_due_at == kNow + 240);

    weather_update = finish_network_boot_weather_attempt(
        &state,
        true,
        false,
        false,
        kNow + 240);
    assert(!weather_update.retry_scheduled);
    assert(weather_update.retry_exhausted);
    assert(weather_update.failure_count == 3);
    assert(!state.weather_due);
    assert(state.weather_due_at == 0);
    assert(state.weather_failures == 0);

    schedule_network_boot_refreshes(
        &state,
        false,
        true,
        0,
        kNow);
    NetworkBootSayingAttemptUpdate saying_update =
        finish_network_boot_saying_attempt(
            &state,
            true,
            false,
            kNow);
    assert(saying_update.retry_scheduled);
    assert(!saying_update.retry_exhausted);
    assert(saying_update.delay_seconds == 120);
    assert(saying_update.failure_count == 1);
    assert(state.saying_due);
    assert(state.saying_due_at == kNow + 120);
    assert(state.saying_failures == 1);

    saying_update = finish_network_boot_saying_attempt(
        &state,
        true,
        true,
        kNow + 120);
    assert(!saying_update.retry_scheduled);
    assert(!saying_update.retry_exhausted);
    assert(!state.saying_due);
    assert(state.saying_due_at == 0);
    assert(state.saying_failures == 0);

    schedule_network_boot_refreshes(
        &state,
        false,
        true,
        kWeatherDueAt,
        kSayingDueAt);
    assert(!stagger_network_boot_saying(&state, false, kNow, 8));
    assert(state.saying_due_at == kSayingDueAt);
    assert(stagger_network_boot_saying(&state, true, kNow, 8));
    assert(state.saying_due_at == kNow + 8);

    clear_network_boot_refreshes(&state);
    assert(!state.weather_due);
    assert(!state.saying_due);
    assert(state.weather_due_at == 0);
    assert(state.saying_due_at == 0);
    assert(state.https_memory_deferrals == 0);
    assert(state.weather_resource_deferrals == 0);
    assert(state.weather_failures == 0);
    assert(state.saying_failures == 0);

    return 0;
}
