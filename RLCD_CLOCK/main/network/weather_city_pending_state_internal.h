// 声明小智天气城市待保存状态仅供事务所有者使用的写入接口。
#pragma once

#include "weather_city_pending_state.h"

bool weather_city_pending_state_init();
bool weather_city_pending_store(const char *city);
bool weather_city_pending_clear(uint32_t generation);
