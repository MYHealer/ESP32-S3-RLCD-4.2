// 声明 QWeather 城市查询首项的字段解析接口。
#pragma once

#include "cJSON.h"

#include <stddef.h>

bool parse_qweather_city_location(const cJSON *location,
                                  char *city_id,
                                  size_t city_id_len,
                                  char *city_name,
                                  size_t city_name_len,
                                  char *lat_out,
                                  size_t lat_len,
                                  char *lon_out,
                                  size_t lon_len);
