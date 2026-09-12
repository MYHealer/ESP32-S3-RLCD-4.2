// 实现闹钟覆盖二次确认匹配、版本保护和超时判断。
#include "alarm_replacement_policy.h"

namespace {
bool confirmation_matches(const AlarmReplacementConfirmation &confirmation,
                          uint32_t existing_version,
                          int requested_hour,
                          int requested_minute,
                          uint32_t now_ms,
                          uint32_t timeout_ms)
{
    return confirmation.pending &&
           confirmation.requested_hour == requested_hour &&
           confirmation.requested_minute == requested_minute &&
           confirmation.existing_version == existing_version &&
           now_ms - confirmation.created_at_ms <= timeout_ms;
}

void remember_confirmation(AlarmReplacementConfirmation *confirmation,
                           uint32_t existing_version,
                           int requested_hour,
                           int requested_minute,
                           uint32_t now_ms)
{
    confirmation->pending = true;
    confirmation->requested_hour = static_cast<uint8_t>(requested_hour);
    confirmation->requested_minute = static_cast<uint8_t>(requested_minute);
    confirmation->existing_version = existing_version;
    confirmation->created_at_ms = now_ms;
}
} // namespace

void clear_alarm_replacement_confirmation(AlarmReplacementConfirmation *confirmation)
{
    if (confirmation) {
        *confirmation = {};
    }
}

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
    AlarmReplacementConfirmation *confirmation)
{
    if (!confirmation) {
        return kAlarmReplacementConfirmationInvalid;
    }
    if (!existing_enabled ||
        (existing_hour == requested_hour && existing_minute == requested_minute)) {
        clear_alarm_replacement_confirmation(confirmation);
        return kAlarmReplacementAccepted;
    }
    if (!confirmed) {
        remember_confirmation(confirmation,
                              existing_version,
                              requested_hour,
                              requested_minute,
                              now_ms);
        return kAlarmReplacementConfirmationRequired;
    }
    if (confirmation_matches(*confirmation,
                             existing_version,
                             requested_hour,
                             requested_minute,
                             now_ms,
                             timeout_ms)) {
        clear_alarm_replacement_confirmation(confirmation);
        return kAlarmReplacementAccepted;
    }
    remember_confirmation(confirmation,
                          existing_version,
                          requested_hour,
                          requested_minute,
                          now_ms);
    return kAlarmReplacementConfirmationInvalid;
}
