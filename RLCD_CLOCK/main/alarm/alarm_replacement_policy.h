// 声明已有单次闹钟被语音请求覆盖时的二次确认状态机。
#pragma once

#include <stdint.h>

enum AlarmReplacementDecision {
    kAlarmReplacementAccepted = 0,
    kAlarmReplacementConfirmationRequired,
    kAlarmReplacementConfirmationInvalid,
};

struct AlarmReplacementConfirmation {
    bool pending;
    uint8_t requested_hour;
    uint8_t requested_minute;
    uint32_t existing_version;
    uint32_t created_at_ms;
};

void clear_alarm_replacement_confirmation(AlarmReplacementConfirmation *confirmation);
AlarmReplacementDecision evaluate_alarm_replacement(
    bool existing_enabled,
    int existing_hour,
    int existing_minute,
    uint32_t existing_version,
    int requested_hour,
    int requested_minute,
    bool confirmed,
    uint32_t now_ms,
    uint32_t timeout_ms,
    AlarmReplacementConfirmation *confirmation);
