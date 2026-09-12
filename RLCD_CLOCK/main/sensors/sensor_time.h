// 声明传感器、UI、闹钟和 NTP 共用的本地时间判断与边界计算。
#pragma once

#include <time.h>

bool is_system_time_plausible(struct tm *local_out = nullptr);
bool is_tm_plausible(const struct tm &local);
bool is_night_slow_window(const struct tm &local);
int periodic_sample_minutes(const struct tm &local, int day_minutes, int night_minutes);
int seconds_until_next_periodic_sample(const struct tm &local, int interval_seconds);
time_t hour_start_from_time(time_t value);
time_t next_local_midnight_time(time_t from);
