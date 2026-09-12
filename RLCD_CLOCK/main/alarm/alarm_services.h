// 管理单次本地闹钟、NVS 持久化、后台触发和按键停止。
#pragma once

#include <stdint.h>

struct AlarmSnapshot {
    bool enabled;
    bool ringing;
    uint8_t hour;
    uint8_t minute;
};

void alarm_task(void *);
void alarm_get_snapshot(AlarmSnapshot *out);
bool alarm_is_enabled();
bool alarm_disable();
bool alarm_stop_ringing_from_button();
void alarm_notify_time_changed();
bool alarm_clear_saved_state();
bool alarm_save_pending();
bool alarm_flush_pending_save();
