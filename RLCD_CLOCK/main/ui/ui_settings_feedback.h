// 声明设置页反馈和手动网络同步状态的共享接口。
#pragma once

#include "ui_settings_contract.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t kSettingsFeedbackTextLen = 48;

struct SettingsFeedbackSnapshot {
    char text[kSettingsFeedbackTextLen] = {};
    bool active = false;
};

struct SettingsUiTimingSnapshot {
    TickType_t feedback_until_tick = 0;
    TickType_t sync_deadline_tick = 0;
    bool sync_busy = false;
};

struct SettingsSyncRequestSnapshot {
    SettingsSyncOp operation = kSettingsSyncNone;
    TickType_t deadline_tick = 0;
    uint32_t generation = 0;
    EventBits_t request_bit = 0;
    uint32_t request_generation = 0;
};

void set_settings_feedback(const char *text, uint32_t duration_ms);
void clear_settings_feedback();
// 读取成功才改写输出；互斥读取失败时保留调用方已有反馈。
bool settings_feedback_snapshot_load(TickType_t now,
                                     SettingsFeedbackSnapshot *out);
SettingsUiTimingSnapshot settings_ui_timing_snapshot_load();
SettingsSyncRequestSnapshot settings_sync_request_snapshot_load();
bool is_settings_sync_busy();
void begin_settings_sync(SettingsSyncOp op,
                         const char *text,
                         EventBits_t request_bit);
void finish_settings_sync(SettingsSyncOp op,
                          uint32_t generation,
                          const char *text);
bool finish_settings_sync_if_timed_out(TickType_t now);
