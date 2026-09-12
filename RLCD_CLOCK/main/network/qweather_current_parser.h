// 声明 QWeather 实时天气和空气质量 now 对象的事务解析接口。
#pragma once

#include "weather_types.h"

#include "cJSON.h"

// 失败不修改输出；成功只覆盖实况字段，保留城市和经纬度。
bool parse_qweather_current_weather(const cJSON *now, WeatherData *weather);
// 解析新版空气质量响应根对象；失败不修改输出，成功保留 ready 和 updated_at。
bool parse_qweather_current_air(const cJSON *root, WeatherAirData *air);
