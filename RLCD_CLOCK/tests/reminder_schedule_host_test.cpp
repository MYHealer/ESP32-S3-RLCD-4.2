// 验证闹钟与番茄钟的本地分钟冲突边界。
#include "alarm_replacement_policy.h"
#include "pomodoro_services.h"
#include "reminder_schedule.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

static int64_t local_time_ms(int hour, int minute, int second, int millisecond)
{
    struct tm value = {};
    value.tm_year = 2026 - 1900;
    value.tm_mon = 7 - 1;
    value.tm_mday = 12;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    return static_cast<int64_t>(mktime(&value)) * 1000 + millisecond;
}

int main()
{
    setenv("TZ", "Asia/Shanghai", 1);
    tzset();

    assert(alarm_time_valid(0, 0));
    assert(alarm_time_valid(23, 59));
    assert(!alarm_time_valid(-1, 0));
    assert(!alarm_time_valid(24, 0));
    assert(!alarm_time_valid(0, -1));
    assert(!alarm_time_valid(0, 60));

    int64_t now = local_time_ms(12, 58, 30, 500);
    assert(reminder_targets_same_local_minute(now, 13, 0, 90U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 13, 1, 90U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 13, 0, 89U * 1000U));

    now = local_time_ms(23, 59, 30, 0);
    assert(reminder_targets_same_local_minute(now, 0, 0, 30U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 0, 1, 30U * 1000U));

    now = local_time_ms(13, 0, 15, 0);
    assert(reminder_targets_same_local_minute(now, 13, 0, 10U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 13, 0, 24U * 60U * 60U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 25, 0, 10U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 13, 60, 10U * 1000U));
    assert(!reminder_targets_same_local_minute(now, 13, 0, 0));
    assert(!reminder_targets_same_local_minute(INT64_MAX, 13, 0, 1));
    assert(!reminder_targets_same_local_minute(INT64_MAX - 9, 13, 0, 10));

    assert(pomodoro_display_seconds(60000U) == 60U);
    assert(pomodoro_display_seconds(59999U) == 60U);
    assert(pomodoro_display_seconds(59000U) == 59U);
    assert(pomodoro_display_seconds(1U) == 1U);
    assert(pomodoro_display_seconds(0U) == 0U);
    assert(pomodoro_next_display_boundary_ms(60000U) == 1000U);
    assert(pomodoro_next_display_boundary_ms(59990U) == 990U);
    assert(pomodoro_next_display_boundary_ms(1U) == 1U);
    assert(pomodoro_next_display_boundary_ms(0U) == 0U);

    constexpr uint32_t kConfirmationTimeoutMs = 120000;
    AlarmReplacementConfirmation confirmation = {};
    assert(evaluate_alarm_replacement(false,
                                      0,
                                      0,
                                      1,
                                      7,
                                      30,
                                      false,
                                      1000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementAccepted);
    assert(!confirmation.pending);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      2,
                                      7,
                                      30,
                                      false,
                                      2000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementAccepted);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      2,
                                      8,
                                      45,
                                      false,
                                      3000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationRequired);
    assert(confirmation.pending);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      2,
                                      8,
                                      45,
                                      true,
                                      4000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementAccepted);
    assert(!confirmation.pending);

    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      3,
                                      9,
                                      0,
                                      false,
                                      5000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationRequired);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      4,
                                      9,
                                      0,
                                      true,
                                      6000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationInvalid);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      4,
                                      9,
                                      0,
                                      true,
                                      6001,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementAccepted);

    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      5,
                                      9,
                                      30,
                                      false,
                                      10000,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationRequired);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      5,
                                      9,
                                      30,
                                      true,
                                      130001,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationInvalid);

    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      6,
                                      10,
                                      0,
                                      false,
                                      UINT32_MAX - 20U,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementConfirmationRequired);
    assert(evaluate_alarm_replacement(true,
                                      7,
                                      30,
                                      6,
                                      10,
                                      0,
                                      true,
                                      30,
                                      kConfirmationTimeoutMs,
                                      &confirmation) == kAlarmReplacementAccepted);
    return 0;
}
