// 声明 NTP 同步和最近成功同步时间查询接口。
#pragma once

#include <time.h>

bool perform_ntp_sync(int max_retries = 30);
time_t get_last_ntp_sync_time();
