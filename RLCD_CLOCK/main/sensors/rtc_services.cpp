// 负责 RTC 时间校验、系统时间恢复和系统时间回写。
#include "rtc_services.h"

#include "app_metadata.h"
#include "app_time_constants.h"
#include "i2c_equipment.h"
#include "sensor_time.h"

#include <esp_log.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define RTC_INVALID_TIME_LOG_FORMAT "ignore invalid RTC time: %04u-%02u-%02u %02u:%02u:%02u"
#define RTC_MKTIME_FAILED_LOG_FORMAT "ignore RTC time: mktime failed"
#define RTC_LOCALTIME_FAILED_LOG_FORMAT "ignore RTC time: localtime normalization failed"
#define RTC_NORMALIZED_MISMATCH_LOG_FORMAT "ignore normalized RTC time mismatch"
#define RTC_SETTIME_FAILED_LOG_FORMAT "set system time from RTC failed errno=%d"
#define RTC_RESTORED_LOG_FORMAT "system time restored from RTC: %04u-%02u-%02u %02u:%02u:%02u"
#define RTC_SYNC_LOCALTIME_FAILED_LOG_FORMAT "skip RTC sync: localtime failed"
#define RTC_SYNC_TIME_NOT_PLAUSIBLE_LOG_FORMAT "skip RTC sync: system time is not plausible"

namespace {
constexpr uint16_t kRtcMinMonth = 1;
constexpr uint16_t kRtcMaxMonth = 12;
constexpr uint16_t kRtcMinDay = 1;
constexpr uint16_t kRtcMaxDay = 31;
constexpr uint16_t kRtcMaxHour = 23;
constexpr uint16_t kRtcMaxMinute = 59;
constexpr uint16_t kRtcMaxSecond = 59;

static_assert(kMinValidYear <= kMaxValidYear, "valid year range must be ordered");

bool rtc_time_fields_in_range(const rtcTimeStruct_t &rtc_time)
{
    return rtc_time.year >= kMinValidYear &&
           rtc_time.year <= kMaxValidYear &&
           rtc_time.month >= kRtcMinMonth &&
           rtc_time.month <= kRtcMaxMonth &&
           rtc_time.day >= kRtcMinDay &&
           rtc_time.day <= kRtcMaxDay &&
           rtc_time.hour <= kRtcMaxHour &&
           rtc_time.minute <= kRtcMaxMinute &&
           rtc_time.second <= kRtcMaxSecond;
}

bool rtc_date_matches_tm(const rtcTimeStruct_t &rtc_time, const struct tm &local_time)
{
    return local_time.tm_year + kTmYearOffset == rtc_time.year &&
           local_time.tm_mon + kTmMonthOffset == rtc_time.month &&
           local_time.tm_mday == rtc_time.day;
}

void copy_rtc_time_to_tm(const rtcTimeStruct_t &rtc_time, struct tm *tm_time)
{
    if (!tm_time) {
        return;
    }
    tm_time->tm_year = rtc_time.year - kTmYearOffset;
    tm_time->tm_mon = rtc_time.month - kTmMonthOffset;
    tm_time->tm_mday = rtc_time.day;
    tm_time->tm_hour = rtc_time.hour;
    tm_time->tm_min = rtc_time.minute;
    tm_time->tm_sec = rtc_time.second;
}
} // namespace

void restore_system_time_from_rtc()
{
    rtcTimeStruct_t rtc_time = {};
    Rtc_GetTime(&rtc_time);
    if (!rtc_time_fields_in_range(rtc_time)) {
        ESP_LOGW(TAG, RTC_INVALID_TIME_LOG_FORMAT,
                 rtc_time.year, rtc_time.month, rtc_time.day,
                 rtc_time.hour, rtc_time.minute, rtc_time.second);
        return;
    }
    struct tm tm_time = {};
    copy_rtc_time_to_tm(rtc_time, &tm_time);
    time_t epoch = mktime(&tm_time);
    if (epoch == (time_t)-1) {
        ESP_LOGW(TAG, RTC_MKTIME_FAILED_LOG_FORMAT);
        return;
    }
    struct tm normalized = {};
    if (!localtime_r(&epoch, &normalized)) {
        ESP_LOGW(TAG, RTC_LOCALTIME_FAILED_LOG_FORMAT);
        return;
    }
    if (!rtc_date_matches_tm(rtc_time, normalized)) {
        ESP_LOGW(TAG, RTC_NORMALIZED_MISMATCH_LOG_FORMAT);
        return;
    }
    struct timeval now = {};
    now.tv_sec = epoch;
    if (settimeofday(&now, nullptr) != 0) {
        ESP_LOGW(TAG, RTC_SETTIME_FAILED_LOG_FORMAT, errno);
        return;
    }
    ESP_LOGI(TAG, RTC_RESTORED_LOG_FORMAT,
             rtc_time.year, rtc_time.month, rtc_time.day,
             rtc_time.hour, rtc_time.minute, rtc_time.second);
}

void sync_rtc_from_system_time()
{
    time_t now;
    time(&now);
    struct tm local = {};
    if (!localtime_r(&now, &local)) {
        ESP_LOGW(TAG, RTC_SYNC_LOCALTIME_FAILED_LOG_FORMAT);
        return;
    }
    if (!is_tm_plausible(local)) {
        ESP_LOGW(TAG, RTC_SYNC_TIME_NOT_PLAUSIBLE_LOG_FORMAT);
        return;
    }
    Rtc_SetTime(local.tm_year + kTmYearOffset,
                local.tm_mon + kTmMonthOffset,
                local.tm_mday,
                local.tm_hour,
                local.tm_min,
                local.tm_sec);
}
