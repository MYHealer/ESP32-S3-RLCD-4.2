// 声明天气共享状态的只读快照接口。
#pragma once

#include "weather_types.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct WeatherAlertStatusSnapshot {
    bool active = false;
    uint8_t count = 0;
    uint32_t version = 0;
};

struct WeatherCacheStatusSnapshot {
    time_t last_sync_time = 0;
    uint32_t version = 0;
    bool extended_data_ready = false;
    bool forecast_data_ready = false;
};

// 读取失败时返回 false，并保留调用方已有输出，便于 UI 继续显示上一份有效缓存。
bool get_weather_full_snapshot(WeatherData *weather,
                               WeatherAlertData *alert,
                               WeatherForecastData *forecast,
                               WeatherAirData *air);
bool get_weather_snapshot(WeatherData *weather);
WeatherAlertStatusSnapshot weather_alert_status_snapshot_load();
uint32_t weather_state_version_load();
bool weather_ready_state_load();
// 读取失败时返回 false，并保留调用方已有缓存元数据。
bool weather_cache_status_snapshot_load(WeatherCacheStatusSnapshot *out);
bool get_weather_alert_title_snapshot(int requested_index,
                                      char *title,
                                      size_t title_len);
