// 声明 NTP 服务内部最近成功时间的并发安全状态接口。
#pragma once

#include <time.h>

bool ntp_runtime_state_init();
time_t ntp_last_sync_time_load();
void ntp_last_sync_time_store(time_t sync_time);
