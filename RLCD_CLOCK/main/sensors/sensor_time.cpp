// 实现跨页面和后台服务共用的本地时间可信度与边界策略。
#include "sensor_time.h"

#include "app_time_constants.h"

namespace {
constexpr int kSecondsPerMinute = 60;
constexpr int kNightSlowWindowStartHour = 22;
constexpr int kNightSlowWindowEndHour = 6;

static_assert(kNightSlowWindowStartHour >= 0 && kNightSlowWindowStartHour < 24,
              "night slow window start hour must be in 0..23");
static_assert(kNightSlowWindowEndHour >= 0 && kNightSlowWindowEndHour < 24,
              "night slow window end hour must be in 0..23");
} // namespace

bool is_system_time_plausible(struct tm *local_out)
{
    time_t now;
    time(&now);
    struct tm local = {};
    localtime_r(&now, &local);
    if (local_out) {
        *local_out = local;
    }
    return is_tm_plausible(local);
}

bool is_tm_plausible(const struct tm &local)
{
    int year = local.tm_year + kTmYearOffset;
    return year >= kMinValidYear && year <= kMaxValidYear;
}

bool is_night_slow_window(const struct tm &local)
{
    return local.tm_hour >= kNightSlowWindowStartHour ||
           local.tm_hour < kNightSlowWindowEndHour;
}

int periodic_sample_minutes(const struct tm &local,
                            int day_minutes,
                            int night_minutes)
{
    return is_night_slow_window(local) ? night_minutes : day_minutes;
}

int seconds_until_next_periodic_sample(const struct tm &local, int interval_seconds)
{
    if (interval_seconds <= 0) {
        return kSecondsPerMinute;
    }
    int seconds_into_hour = local.tm_min * kSecondsPerMinute + local.tm_sec;
    int seconds_to_next = interval_seconds - (seconds_into_hour % interval_seconds);
    if (seconds_to_next <= 0 || seconds_to_next > interval_seconds) {
        return interval_seconds;
    }
    return seconds_to_next;
}

time_t hour_start_from_time(time_t value)
{
    struct tm local = {};
    localtime_r(&value, &local);
    local.tm_min = 0;
    local.tm_sec = 0;
    return mktime(&local);
}

time_t next_local_midnight_time(time_t from)
{
    struct tm local = {};
    if (!localtime_r(&from, &local) || !is_tm_plausible(local)) {
        return 0;
    }
    local.tm_mday += 1;
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    time_t next = mktime(&local);
    return next > from ? next : 0;
}
