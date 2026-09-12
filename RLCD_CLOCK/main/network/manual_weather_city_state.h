// 声明手动天气城市的线程安全运行态快照接口。
#pragma once

#include "weather_city_contract.h"

#include <stddef.h>

bool manual_weather_city_snapshot(char *out, size_t out_len);
bool manual_weather_city_is_configured();
