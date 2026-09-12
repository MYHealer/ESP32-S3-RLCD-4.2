// 提供天气时钟日期、缓存键、进度和整点触发的纯时间计算。
#pragma once

#include <stddef.h>
#include <time.h>

struct ClockUiTimeSnapshot {
    int second_key;
    int minute_key;
    int date_key;
    int hour_key;
    int day_progress_filled;
    const char *weekday;
};

ClockUiTimeSnapshot clock_ui_time_snapshot(const struct tm &local);
bool clock_hourly_chime_due(const struct tm &local,
                            const ClockUiTimeSnapshot &snapshot,
                            bool chime_enabled,
                            bool low_battery_mode,
                            int last_chime_hour_key);
void format_clock_date_text(char *out,
                            size_t out_len,
                            const struct tm &local,
                            const char *weekday);
