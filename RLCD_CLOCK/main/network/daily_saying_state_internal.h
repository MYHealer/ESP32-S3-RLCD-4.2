// 声明每日文字状态初始化和网络生产者使用的内部写入接口。
#pragma once

#include "daily_saying_state.h"

bool daily_saying_state_init();
void reset_daily_saying_cache();
bool daily_saying_state_publish(const char *text, time_t synced_at);
