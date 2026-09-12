// 声明 QWeather 单条预警 JSON 的字段解析接口。
#pragma once

#include "weather_types.h"

#include "cJSON.h"

struct QweatherAlertItem {
    bool title_format_ok = false;
    char title[kWeatherAlertTitleLen] = {};
    int rank = 0;
};

bool parse_qweather_alert_item(const cJSON *item, QweatherAlertItem *parsed);
