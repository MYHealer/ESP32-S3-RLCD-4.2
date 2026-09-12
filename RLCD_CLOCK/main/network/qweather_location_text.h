// 声明天气定位文本格式化、城市提取和坐标回填接口。
#pragma once

#include "weather_types.h"

#include <stddef.h>

inline constexpr size_t kWeatherLocationTextSize = 32;
inline constexpr char kIpLocationInvalidArgLog[] = "ip location invalid arg";
inline constexpr char kIpLocationCoordinateTooLongLog[] = "ip location coordinate text too long";

void copy_first_nonempty_text(char *out,
                              size_t out_len,
                              const char *first,
                              const char *second = nullptr,
                              const char *third = nullptr);
bool format_ip_coordinates(char *out, size_t out_len, double longitude, double latitude);
void copy_ip_coordinate_location(const char *location,
                                 char *city_id,
                                 size_t city_id_len,
                                 WeatherData *weather);
void copy_ip_region_city(char *out, size_t out_len, const char *region);
