// 验证天气时钟缓存键、整天进度、日期文本和整点触发边界。
#include "ui_clock_time.h"

#include <assert.h>
#include <string.h>

namespace {
struct tm make_time(int hour, int minute, int second, int weekday = 0)
{
    struct tm local = {};
    local.tm_year = 126;
    local.tm_mon = 6;
    local.tm_mday = 12;
    local.tm_yday = 192;
    local.tm_wday = weekday;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    return local;
}
} // namespace

int main()
{
    struct tm midnight = make_time(0, 0, 0, 0);
    ClockUiTimeSnapshot start = clock_ui_time_snapshot(midnight);
    assert(start.second_key == 0);
    assert(start.minute_key == 0);
    assert(start.date_key == 20260712);
    assert(start.hour_key == 192 * 24);
    assert(start.day_progress_filled == 0);
    assert(strcmp(start.weekday, "星期日") == 0);

    struct tm noon = make_time(12, 0, 0, 3);
    ClockUiTimeSnapshot middle = clock_ui_time_snapshot(noon);
    assert(middle.second_key == 12 * 60 * 60);
    assert(middle.minute_key == 720);
    assert(middle.day_progress_filled == 30);
    assert(strcmp(middle.weekday, "星期三") == 0);

    struct tm day_end = make_time(23, 59, 59, 6);
    ClockUiTimeSnapshot end = clock_ui_time_snapshot(day_end);
    assert(end.second_key == 24 * 60 * 60 - 1);
    assert(end.minute_key == 1439);
    assert(end.hour_key == 192 * 24 + 23);
    assert(end.day_progress_filled == 59);
    assert(strcmp(end.weekday, "星期六") == 0);

    struct tm invalid_weekday = make_time(8, 30, 0, 7);
    ClockUiTimeSnapshot invalid = clock_ui_time_snapshot(invalid_weekday);
    assert(strcmp(invalid.weekday, "--") == 0);

    char date[48] = {};
    format_clock_date_text(date, sizeof(date), noon, middle.weekday);
    assert(strcmp(date, "2026/07/12 / 星期三") == 0);
    char short_date[3] = {};
    format_clock_date_text(short_date, sizeof(short_date), noon, middle.weekday);
    assert(strcmp(short_date, "--") == 0);

    ClockUiTimeSnapshot chime_snapshot = clock_ui_time_snapshot(midnight);
    assert(clock_hourly_chime_due(midnight, chime_snapshot, true, false, -1));
    assert(!clock_hourly_chime_due(midnight, chime_snapshot, false, false, -1));
    assert(!clock_hourly_chime_due(midnight, chime_snapshot, true, true, -1));
    assert(!clock_hourly_chime_due(midnight,
                                   chime_snapshot,
                                   true,
                                   false,
                                   chime_snapshot.hour_key));
    struct tm last_window_second = make_time(6, 0, 2);
    ClockUiTimeSnapshot last_window = clock_ui_time_snapshot(last_window_second);
    assert(clock_hourly_chime_due(last_window_second, last_window, true, false, -1));
    struct tm after_window = make_time(6, 0, 3);
    ClockUiTimeSnapshot after = clock_ui_time_snapshot(after_window);
    assert(!clock_hourly_chime_due(after_window, after, true, false, -1));
    struct tm nonzero_minute = make_time(6, 1, 0);
    ClockUiTimeSnapshot later = clock_ui_time_snapshot(nonzero_minute);
    assert(!clock_hourly_chime_due(nonzero_minute, later, true, false, -1));
    return 0;
}
