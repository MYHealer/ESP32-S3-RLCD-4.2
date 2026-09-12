// 计算单次闹钟下一次触发分钟与相对计时结束分钟是否冲突。
#include "reminder_schedule.h"

#include <sys/time.h>
#include <time.h>

namespace {
constexpr int kHoursPerDay = 24;
constexpr int kMinutesPerHour = 60;
constexpr int64_t kMillisecondsPerSecond = 1000;

bool same_local_minute(time_t left, time_t right)
{
    struct tm left_local = {};
    struct tm right_local = {};
    if (!localtime_r(&left, &left_local) || !localtime_r(&right, &right_local)) {
        return false;
    }
    return left_local.tm_year == right_local.tm_year &&
           left_local.tm_yday == right_local.tm_yday &&
           left_local.tm_hour == right_local.tm_hour &&
           left_local.tm_min == right_local.tm_min;
}

bool next_alarm_minute(time_t now, int hour, int minute, time_t *out)
{
    if (!out) {
        return false;
    }
    struct tm current = {};
    if (!localtime_r(&now, &current)) {
        return false;
    }
    struct tm candidate = current;
    candidate.tm_hour = hour;
    candidate.tm_min = minute;
    candidate.tm_sec = 0;
    candidate.tm_isdst = -1;
    time_t candidate_time = mktime(&candidate);
    if (candidate_time == static_cast<time_t>(-1)) {
        return false;
    }

    struct tm current_minute = current;
    current_minute.tm_sec = 0;
    current_minute.tm_isdst = -1;
    time_t current_minute_time = mktime(&current_minute);
    if (current_minute_time == static_cast<time_t>(-1)) {
        return false;
    }
    if (candidate_time < current_minute_time) {
        candidate.tm_mday += 1;
        candidate_time = mktime(&candidate);
        if (candidate_time == static_cast<time_t>(-1)) {
            return false;
        }
    }
    *out = candidate_time;
    return true;
}

bool delayed_wall_clock_ms_valid(int64_t now_ms, uint32_t delay_ms)
{
    return now_ms <= INT64_MAX - static_cast<int64_t>(delay_ms);
}
} // namespace

bool alarm_time_valid(int hour, int minute)
{
    return hour >= 0 && hour < kHoursPerDay &&
           minute >= 0 && minute < kMinutesPerHour;
}

bool reminder_targets_same_local_minute(int64_t now_ms,
                                        int alarm_hour,
                                        int alarm_minute,
                                        uint32_t delay_ms)
{
    if (now_ms < 0 || delay_ms == 0 ||
        !alarm_time_valid(alarm_hour, alarm_minute) ||
        !delayed_wall_clock_ms_valid(now_ms, delay_ms)) {
        return false;
    }
    time_t now = static_cast<time_t>(now_ms / kMillisecondsPerSecond);
    time_t alarm_time = 0;
    if (!next_alarm_minute(now, alarm_hour, alarm_minute, &alarm_time)) {
        return false;
    }
    time_t delayed_time = static_cast<time_t>((now_ms + delay_ms) / kMillisecondsPerSecond);
    return same_local_minute(alarm_time, delayed_time);
}

int64_t reminder_wall_clock_ms()
{
    struct timeval now = {};
    gettimeofday(&now, nullptr);
    return static_cast<int64_t>(now.tv_sec) * kMillisecondsPerSecond +
           now.tv_usec / kMillisecondsPerSecond;
}
