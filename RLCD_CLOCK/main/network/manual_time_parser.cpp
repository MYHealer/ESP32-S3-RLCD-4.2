// 实现配网页手动离线时间的纯解析与校验。
#include "manual_time_parser.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_time_constants.h"

#include <esp_log.h>

#include <stdio.h>

namespace {
constexpr int kManualTimeMinMonth = 1;
constexpr int kManualTimeMaxMonth = 12;
constexpr int kManualTimeMinDay = 1;
constexpr int kManualTimeMaxDay = 31;
constexpr int kManualTimeMinHour = 0;
constexpr int kManualTimeMaxHour = 23;
constexpr int kManualTimeMinMinute = 0;
constexpr int kManualTimeMaxMinute = 59;
constexpr int kManualTimeMinSecond = 0;
constexpr int kManualTimeMaxSecond = 59;
constexpr int kManualTimeRequiredFieldCount = 5;
constexpr const char *kManualTimeIsoSecondsFormat = "%d-%d-%dT%d:%d:%d";
constexpr const char *kManualTimeSpaceSecondsFormat = "%d-%d-%d %d:%d:%d";
constexpr const char *kManualTimeSpaceMinutesFormat = "%d-%d-%d %d:%d";
#define MANUAL_TIME_NORMALIZATION_FAILED_LOG "manual offline time localtime normalization failed"

bool value_in_range(int value, int min_value, int max_value)
{
    return value >= min_value && value <= max_value;
}

bool manual_datetime_fields_in_range(int year, int month, int day, int hour, int minute, int second)
{
    return value_in_range(year, kMinValidYear, kMaxValidYear) &&
           value_in_range(month, kManualTimeMinMonth, kManualTimeMaxMonth) &&
           value_in_range(day, kManualTimeMinDay, kManualTimeMaxDay) &&
           value_in_range(hour, kManualTimeMinHour, kManualTimeMaxHour) &&
           value_in_range(minute, kManualTimeMinMinute, kManualTimeMaxMinute) &&
           value_in_range(second, kManualTimeMinSecond, kManualTimeMaxSecond);
}

int try_parse_manual_datetime_format(const char *text,
                                     const char *format,
                                     bool has_seconds,
                                     int *year,
                                     int *month,
                                     int *day,
                                     int *hour,
                                     int *minute,
                                     int *second)
{
    if (!text || !format || !year || !month || !day || !hour || !minute || !second) {
        return 0;
    }
    *second = 0;
    return has_seconds
               ? sscanf(text, format, year, month, day, hour, minute, second)
               : sscanf(text, format, year, month, day, hour, minute);
}

int parse_manual_datetime_fields(const char *text,
                                 int *year,
                                 int *month,
                                 int *day,
                                 int *hour,
                                 int *minute,
                                 int *second)
{
    int parsed = try_parse_manual_datetime_format(text,
                                                  kManualTimeIsoSecondsFormat,
                                                  true,
                                                  year,
                                                  month,
                                                  day,
                                                  hour,
                                                  minute,
                                                  second);
    if (parsed >= kManualTimeRequiredFieldCount) {
        return parsed;
    }
    parsed = try_parse_manual_datetime_format(text,
                                              kManualTimeSpaceSecondsFormat,
                                              true,
                                              year,
                                              month,
                                              day,
                                              hour,
                                              minute,
                                              second);
    if (parsed >= kManualTimeRequiredFieldCount) {
        return parsed;
    }
    return try_parse_manual_datetime_format(text,
                                            kManualTimeSpaceMinutesFormat,
                                            false,
                                            year,
                                            month,
                                            day,
                                            hour,
                                            minute,
                                            second);
}

void fill_manual_datetime_tm(struct tm *local, int year, int month, int day, int hour, int minute, int second)
{
    if (!local) {
        return;
    }
    *local = {};
    local->tm_year = year - kManualTimeTmYearOffset;
    local->tm_mon = month - kManualTimeTmMonthOffset;
    local->tm_mday = day;
    local->tm_hour = hour;
    local->tm_min = minute;
    local->tm_sec = second;
    local->tm_isdst = -1;
}

bool manual_datetime_normalizes(struct tm local, struct tm *normalized)
{
    time_t epoch = mktime(&local);
    if (epoch <= 0) {
        return false;
    }
    struct tm next = {};
    if (!localtime_r(&epoch, &next)) {
        ESP_LOGW(TAG, "%s", MANUAL_TIME_NORMALIZATION_FAILED_LOG);
        return false;
    }
    if (next.tm_year != local.tm_year ||
        next.tm_mon != local.tm_mon ||
        next.tm_mday != local.tm_mday ||
        next.tm_hour != local.tm_hour ||
        next.tm_min != local.tm_min) {
        return false;
    }
    if (normalized) {
        *normalized = next;
    }
    return true;
}

static_assert(kManualTimeRequiredFieldCount == 5, "manual time requires year/month/day/hour/minute");
static_assert(kManualTimeMinMonth == 1 && kManualTimeMaxMonth == 12, "manual time month range must be 1..12");
static_assert(kManualTimeMinDay == 1 && kManualTimeMaxDay == 31, "manual time day range must be 1..31");
static_assert(kManualTimeMinHour == 0 && kManualTimeMaxHour == 23, "manual time hour range must be 0..23");
static_assert(kManualTimeMinMinute == 0 && kManualTimeMaxMinute == 59,
              "manual time minute range must be 0..59");
static_assert(kManualTimeMinSecond == 0 && kManualTimeMaxSecond == 59,
              "manual time second range must be 0..59");
static_assert(kManualTimeTmYearOffset == 1900, "struct tm year offset must stay 1900");
static_assert(kManualTimeTmMonthOffset == 1, "struct tm month offset must stay 1");
static_assert(cstr_nonempty(kManualTimeIsoSecondsFormat), "manual ISO time format must be non-empty");
static_assert(cstr_nonempty(kManualTimeSpaceSecondsFormat), "manual space time format must be non-empty");
static_assert(cstr_nonempty(kManualTimeSpaceMinutesFormat), "manual minute time format must be non-empty");
} // namespace

bool parse_manual_datetime_text(const char *text, struct tm *out)
{
    if (!text || !out || text[0] == '\0') {
        return false;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int parsed = parse_manual_datetime_fields(text, &year, &month, &day, &hour, &minute, &second);
    if (parsed < kManualTimeRequiredFieldCount ||
        !manual_datetime_fields_in_range(year, month, day, hour, minute, second)) {
        return false;
    }
    struct tm local = {};
    fill_manual_datetime_tm(&local, year, month, day, hour, minute, second);
    return manual_datetime_normalizes(local, out);
}
