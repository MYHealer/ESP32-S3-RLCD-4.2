// 声明联网任务私有的启动天气、每日文字与 HTTPS 资源退避运行态。
#pragma once

#include <stdint.h>
#include <time.h>

struct NetworkBootHttpsDeferralInput;
struct NetworkSyncSchedule;

struct NetworkBootRefreshState {
    time_t weather_due_at = 0;
    time_t saying_due_at = 0;
    uint8_t https_memory_deferrals = 0;
    uint8_t weather_resource_deferrals = 0;
    uint8_t weather_failures = 0;
    uint8_t saying_failures = 0;
    bool weather_due = false;
    bool saying_due = false;
};

struct NetworkBootHttpsMemoryDeferralUpdate {
    time_t delay_seconds = 0;
    uint8_t deferral_count = 0;
    bool deferred = false;
    bool weather_deferred = false;
    bool saying_deferred = false;
};

struct NetworkBootWeatherAttemptUpdate {
    time_t delay_seconds = 0;
    uint8_t deferral_count = 0;
    uint8_t failure_count = 0;
    bool retry_scheduled = false;
    bool retry_exhausted = false;
};

using NetworkBootSayingAttemptUpdate = NetworkBootWeatherAttemptUpdate;

NetworkBootRefreshState initialize_network_boot_refresh_state(
    bool weather_due,
    bool saying_due,
    time_t weather_due_at,
    time_t saying_due_at);
void schedule_network_boot_refreshes(
    NetworkBootRefreshState *state,
    bool weather_due,
    bool saying_due,
    time_t weather_due_at,
    time_t saying_due_at);
void clear_network_boot_refreshes(NetworkBootRefreshState *state);
void reconcile_network_boot_refresh_state(
    NetworkBootRefreshState *state,
    bool weather_page_enabled,
    bool saying_page_enabled,
    bool weather_cache_current,
    bool saying_cache_current,
    bool blocked);
NetworkBootHttpsMemoryDeferralUpdate defer_network_boot_refreshes_for_memory(
    NetworkBootRefreshState *state,
    NetworkSyncSchedule *schedule,
    NetworkBootHttpsDeferralInput input,
    bool memory_allowed);
NetworkBootWeatherAttemptUpdate finish_network_boot_weather_attempt(
    NetworkBootRefreshState *state,
    bool weather_ready,
    bool succeeded,
    bool resource_deferred,
    time_t now);
NetworkBootSayingAttemptUpdate finish_network_boot_saying_attempt(
    NetworkBootRefreshState *state,
    bool saying_ready,
    bool succeeded,
    time_t now);
bool stagger_network_boot_saying(
    NetworkBootRefreshState *state,
    bool should_stagger,
    time_t now,
    time_t gap_seconds);
