// 声明仅供闹钟服务实现使用的运行态与覆盖确认接口。
#pragma once

#include "alarm_replacement_policy.h"
#include "alarm_services.h"

#include <stdint.h>

struct AlarmPendingSaveSnapshot {
    bool pending;
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint32_t generation;
};

bool alarm_runtime_state_init();
bool alarm_runtime_publish(bool enabled,
                           bool ringing,
                           int hour,
                           int minute);
bool alarm_runtime_publish_deferred_save(bool enabled,
                                         bool ringing,
                                         int hour,
                                         int minute);
bool alarm_runtime_snapshot(AlarmSnapshot *out);
bool alarm_runtime_is_enabled();
bool alarm_runtime_pending_save_snapshot(AlarmPendingSaveSnapshot *out);
bool alarm_runtime_pending_save_exists();
bool alarm_runtime_pending_save_clear(uint32_t generation);
bool alarm_runtime_pending_save_discard();
bool alarm_runtime_clear_replacement();
AlarmReplacementDecision alarm_runtime_replacement_decision(
    int requested_hour,
    int requested_minute,
    bool confirmed,
    uint32_t now_ms,
    uint32_t timeout_ms,
    AlarmSnapshot *existing);
