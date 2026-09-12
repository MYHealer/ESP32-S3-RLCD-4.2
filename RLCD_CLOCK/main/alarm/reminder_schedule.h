// 声明闹钟与相对计时任务的本地分钟冲突计算。
#pragma once

#include <stdint.h>

bool alarm_time_valid(int hour, int minute);
bool reminder_targets_same_local_minute(int64_t now_ms,
                                        int alarm_hour,
                                        int alarm_minute,
                                        uint32_t delay_ms);
int64_t reminder_wall_clock_ms();
