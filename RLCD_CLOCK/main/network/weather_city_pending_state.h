// 声明小智天气城市待保存请求的线程安全快照与代次清理接口。
#pragma once

#include "weather_city_contract.h"

#include <stdint.h>

struct WeatherCityPendingSnapshot {
    char city[kManualWeatherCityLen];
    bool pending;
    uint32_t generation;
};

bool weather_city_pending_snapshot(WeatherCityPendingSnapshot *out);
bool weather_city_pending_exists();
