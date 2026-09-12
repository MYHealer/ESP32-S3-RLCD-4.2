// 声明 QWeather 预警颜色、标题压缩和排序写入接口。
#pragma once

#include "weather_types.h"

#include <stddef.h>

inline constexpr char kWeatherAlertSuffix[] = "预警";

const char *warning_color_name(const char *code);
int warning_color_rank(const char *code);
bool build_weather_alert_title(char *title,
                               size_t title_len,
                               const char *event_name,
                               const char *color_code,
                               const char *headline);
void add_weather_alert_title(WeatherAlertData *alert, const char *title, int rank);
