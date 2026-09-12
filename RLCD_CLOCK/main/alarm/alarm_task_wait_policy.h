// 计算闹钟后台任务到下一次目标时刻的低功耗等待时间。
#pragma once

#include <stdint.h>

inline constexpr uint32_t kAlarmMinuteMs = 60U * 1000U;
inline constexpr uint32_t kAlarmHourMs = 60U * kAlarmMinuteMs;
inline constexpr uint32_t kAlarmDayMs = 24U * kAlarmHourMs;
inline constexpr uint32_t kAlarmSaveRetryBaseMs = kAlarmMinuteMs;
inline constexpr uint32_t kAlarmSaveRetryMaximumMs = kAlarmHourMs;

constexpr uint32_t alarm_save_retry_delay_ms(uint8_t failure_count)
{
    uint32_t delay_ms = kAlarmSaveRetryBaseMs;
    uint8_t remaining_doublings = failure_count > 1 ? failure_count - 1 : 0;
    while (remaining_doublings > 0 && delay_ms < kAlarmSaveRetryMaximumMs) {
        if (delay_ms > kAlarmSaveRetryMaximumMs / 2U) {
            return kAlarmSaveRetryMaximumMs;
        }
        delay_ms *= 2U;
        --remaining_doublings;
    }
    return delay_ms < kAlarmSaveRetryMaximumMs
               ? delay_ms
               : kAlarmSaveRetryMaximumMs;
}

constexpr bool alarm_wait_clock_fields_valid(int current_hour,
                                             int current_minute,
                                             int current_second,
                                             int current_millisecond,
                                             int alarm_hour,
                                             int alarm_minute)
{
    return current_hour >= 0 && current_hour < 24 &&
           current_minute >= 0 && current_minute < 60 &&
           current_second >= 0 && current_second < 60 &&
           current_millisecond >= 0 && current_millisecond < 1000 &&
           alarm_hour >= 0 && alarm_hour < 24 &&
           alarm_minute >= 0 && alarm_minute < 60;
}

constexpr uint32_t alarm_task_wait_ms(bool alarm_enabled,
                                      bool time_valid,
                                      int current_hour,
                                      int current_minute,
                                      int current_second,
                                      int current_millisecond,
                                      int alarm_hour,
                                      int alarm_minute)
{
    if (!alarm_enabled || !time_valid ||
        !alarm_wait_clock_fields_valid(current_hour,
                                       current_minute,
                                       current_second,
                                       current_millisecond,
                                       alarm_hour,
                                       alarm_minute)) {
        return 0;
    }

    const uint32_t current_ms =
        static_cast<uint32_t>(current_hour) * kAlarmHourMs +
        static_cast<uint32_t>(current_minute) * kAlarmMinuteMs +
        static_cast<uint32_t>(current_second) * 1000U +
        static_cast<uint32_t>(current_millisecond);
    uint32_t target_ms =
        static_cast<uint32_t>(alarm_hour) * kAlarmHourMs +
        static_cast<uint32_t>(alarm_minute) * kAlarmMinuteMs;
    if (target_ms <= current_ms) {
        target_ms += kAlarmDayMs;
    }
    return target_ms - current_ms;
}

static_assert(kAlarmHourMs == 60U * 60U * 1000U,
              "alarm hour duration must remain one hour");
static_assert(kAlarmDayMs == 24U * 60U * 60U * 1000U,
              "alarm day duration must remain one day");
static_assert(alarm_save_retry_delay_ms(7) == kAlarmSaveRetryMaximumMs,
              "alarm persistence retry must reach its one-hour cap");
