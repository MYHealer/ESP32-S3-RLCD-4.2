// 验证共享时间可信度、夜间窗口、采样边界和本地时间边界。
#include "sensor_time.h"

#include "app_time_constants.h"

#include <assert.h>
#include <stdlib.h>

namespace {
struct tm local_value(int year, int month, int day, int hour, int minute, int second)
{
    struct tm value = {};
    value.tm_year = year - kTmYearOffset;
    value.tm_mon = month - kTmMonthOffset;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    return value;
}

time_t local_epoch(int year, int month, int day, int hour, int minute, int second)
{
    struct tm value = local_value(year, month, day, hour, minute, second);
    return mktime(&value);
}

void expect_local_time(time_t value,
                       int expected_year,
                       int expected_month,
                       int expected_day,
                       int expected_hour,
                       int expected_minute)
{
    struct tm local = {};
    localtime_r(&value, &local);
    assert(local.tm_year + kTmYearOffset == expected_year);
    assert(local.tm_mon + kTmMonthOffset == expected_month);
    assert(local.tm_mday == expected_day);
    assert(local.tm_hour == expected_hour);
    assert(local.tm_min == expected_minute);
    assert(local.tm_sec == 0);
}
} // namespace

int main()
{
    static_assert(kTmYearOffset == 1900);
    static_assert(kTmMonthOffset == 1);

    setenv("TZ", "Asia/Shanghai", 1);
    tzset();

    assert(!is_tm_plausible(local_value(2023, 12, 31, 23, 59, 59)));
    assert(is_tm_plausible(local_value(2024, 1, 1, 0, 0, 0)));
    assert(is_tm_plausible(local_value(2035, 12, 31, 23, 59, 59)));
    assert(!is_tm_plausible(local_value(2036, 1, 1, 0, 0, 0)));

    assert(is_night_slow_window(local_value(2026, 7, 12, 22, 0, 0)));
    assert(is_night_slow_window(local_value(2026, 7, 12, 5, 59, 59)));
    assert(!is_night_slow_window(local_value(2026, 7, 12, 6, 0, 0)));
    assert(!is_night_slow_window(local_value(2026, 7, 12, 21, 59, 59)));
    assert(periodic_sample_minutes(local_value(2026, 7, 12, 23, 0, 0), 1, 2) == 2);
    assert(periodic_sample_minutes(local_value(2026, 7, 12, 12, 0, 0), 1, 2) == 1);
    int night_sample_seconds = periodic_sample_minutes(
        local_value(2026, 7, 12, 22, 0, 0), 1, 2) * 60;
    assert(seconds_until_next_periodic_sample(
               local_value(2026, 7, 12, 22, 0, 0), night_sample_seconds) == 120);
    assert(seconds_until_next_periodic_sample(
               local_value(2026, 7, 12, 22, 1, 30), night_sample_seconds) == 30);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 0, 0), 60) == 60);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 0, 1), 60) == 59);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 0, 30), 60) == 30);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 1, 59), 60) == 1);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 5, 0), 120) == 60);
    assert(seconds_until_next_periodic_sample(local_value(2026, 7, 12, 12, 5, 0), 0) == 60);

    time_t value = local_epoch(2026, 7, 12, 13, 45, 56);
    expect_local_time(hour_start_from_time(value), 2026, 7, 12, 13, 0);
    expect_local_time(next_local_midnight_time(value), 2026, 7, 13, 0, 0);
    expect_local_time(next_local_midnight_time(local_epoch(2026, 12, 31, 23, 59, 59)),
                      2027, 1, 1, 0, 0);

    time_t invalid = local_epoch(2023, 7, 12, 12, 34, 56);
    assert(next_local_midnight_time(invalid) == 0);
    return 0;
}
