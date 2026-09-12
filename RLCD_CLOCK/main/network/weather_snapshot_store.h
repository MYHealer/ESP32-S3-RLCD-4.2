// 提供不依赖任务或锁实现的天气快照存储与提交规则。
#pragma once

#include "weather_types.h"

struct WeatherSnapshotStore {
    WeatherData weather;
    WeatherAlertData alert;
    WeatherForecastData forecast;
    WeatherAirData air;
    time_t last_sync_time = 0;
    bool extended_refresh_required = false;
};

void weather_snapshot_store_read(const WeatherSnapshotStore &store,
                                 WeatherData *weather,
                                 WeatherAlertData *alert,
                                 WeatherForecastData *forecast,
                                 WeatherAirData *air);
bool weather_snapshot_store_extended_ready(const WeatherSnapshotStore &store);
void weather_snapshot_store_commit(WeatherSnapshotStore *store,
                                   const WeatherData &next,
                                   const WeatherAlertData &next_alert,
                                   const WeatherForecastData &next_forecast,
                                   const WeatherAirData &next_air,
                                   bool alert_updated,
                                   bool forecast_ok,
                                   bool air_ok,
                                   time_t synced_at);
void weather_snapshot_store_commit_basic(WeatherSnapshotStore *store,
                                         const WeatherData &next,
                                         const WeatherAlertData &next_alert,
                                         bool alert_updated,
                                         time_t synced_at);
