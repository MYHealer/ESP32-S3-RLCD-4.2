// 声明工作页可见时的天气与每日文字补拉及天气状态刷新接口。
#pragma once

#include "app_event_group.h"
#include "ui_visible_sync_retry.h"

#include <time.h>

struct WeatherCacheStatusSnapshot;
struct DailySayingCacheSnapshot;

struct ActiveWorkPageState {
    bool history = false;
    bool gallery = false;
    bool calendar = false;
    bool weather_board = false;
    bool flip_clock = false;
    bool xiaozhi = false;
    bool weather_clock = false;
    bool aggregate_clock = false;
    bool uses_weather_data = false;
    bool uses_extended_weather_data = false;
    bool uses_daily_saying = false;
};

ActiveWorkPageState active_work_page_state_for_mode(int active_page,
                                                    bool normal_mode);
void update_visible_weather_sync(const ActiveWorkPageState &state,
                                 time_t now,
                                 TickType_t tick_now,
                                 const WeatherCacheStatusSnapshot *cache_status,
                                 VisibleSyncRetryState<TickType_t> &retry);
void update_visible_daily_saying_sync(const ActiveWorkPageState &state,
                                      time_t now,
                                      TickType_t tick_now,
                                      const DailySayingCacheSnapshot *cache_status,
                                      VisibleSyncRetryState<TickType_t> &retry);
bool update_weather_clock_network_status(EventBits_t bits);
