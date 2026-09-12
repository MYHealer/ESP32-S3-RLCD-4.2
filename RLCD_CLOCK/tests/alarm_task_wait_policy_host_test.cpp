// 验证闹钟任务直接等待到目标时刻，并由通知处理配置和校时变化。
#include "alarm_task_wait_policy.h"

#include <cassert>

int main()
{
    static_assert(kAlarmMinuteMs == 60000U,
                  "alarm minute boundary must remain one minute");
    static_assert(kAlarmDayMs == 86400000U,
                  "alarm target wait must fit one day");
    static_assert(kAlarmSaveRetryBaseMs == 60000U,
                  "deferred alarm persistence must not retry aggressively");
    static_assert(kAlarmSaveRetryMaximumMs == 3600000U,
                  "deferred alarm persistence must cap at one hour");
    static_assert(alarm_save_retry_delay_ms(0) == 60000U,
                  "an unscheduled retry starts at one minute");
    static_assert(alarm_save_retry_delay_ms(1) == 60000U,
                  "the first failed retry waits one minute");
    static_assert(alarm_save_retry_delay_ms(2) == 120000U,
                  "the second failed retry waits two minutes");
    static_assert(alarm_save_retry_delay_ms(6) == 1920000U,
                  "the sixth failed retry waits thirty-two minutes");
    static_assert(alarm_save_retry_delay_ms(7) == 3600000U,
                  "the seventh failed retry is capped at one hour");
    static_assert(alarm_save_retry_delay_ms(255) == 3600000U,
                  "the retry delay must remain capped after saturation");
    static_assert(alarm_task_wait_ms(false, true, 6, 0, 0, 0, 7, 0) == 0,
                  "disabled alarm must sleep until notification");
    static_assert(alarm_task_wait_ms(true, false, 6, 0, 0, 0, 7, 0) == 0,
                  "invalid time must sleep until a time-change notification");
    static_assert(alarm_task_wait_ms(true, true, 6, 0, 0, 0, 7, 0) == 3600000U,
                  "future alarm must wait directly to its target hour");
    static_assert(alarm_task_wait_ms(true, true, 6, 59, 59, 999, 7, 0) == 1U,
                  "last millisecond before target must wait one millisecond");
    static_assert(alarm_task_wait_ms(true, true, 7, 0, 0, 0, 7, 0) == kAlarmDayMs,
                  "a consumed current-minute target must advance to tomorrow");
    static_assert(alarm_task_wait_ms(true, true, 23, 30, 0, 0, 0, 15) == 2700000U,
                  "target calculation must cross midnight");
    static_assert(alarm_task_wait_ms(true, true, 24, 0, 0, 0, 7, 0) == 0,
                  "invalid clock fields must block until notification");

    assert(alarm_task_wait_ms(true, true, 12, 34, 56, 789, 18, 0) == 19503211U);
    assert(alarm_task_wait_ms(false, false, -1, -1, -1, -1, -1, -1) == 0);
    return 0;
}
