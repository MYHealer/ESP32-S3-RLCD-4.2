// 维护设置页反馈文本和手动网络同步状态，不承担设置页绘制。
#include "ui_settings_feedback_internal.h"

#include "app_event_group.h"
#include "app_metadata.h"
#include "app_runtime_timing.h"
#include "app_tick_time.h"
#include "network_diagnostics_state.h"
#include "network_runtime_events.h"
#include "network_sync_request_generation.h"
#include "scoped_semaphore_lock.h"
#include "ui_settings_activity_state.h"
#include "ui_task_notify.h"
#include "ui_text_format.h"

#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace {
StaticTaskMutex s_settings_feedback_mutex;
StaticTaskMutex s_settings_sync_mutex;
SettingsSyncRequestSnapshot s_settings_sync_state;
uint32_t s_settings_sync_generation = 0;
EXT_RAM_BSS_ATTR char s_settings_feedback_text[kSettingsFeedbackTextLen] = {};
TickType_t s_settings_feedback_until_tick = 0;
#define SETTINGS_MANUAL_SYNC_TIMEOUT_LOG_FORMAT "settings manual sync timeout: op=%d"
constexpr const char *kSettingsNtpTimeoutFeedback = "时间同步超时";
constexpr const char *kSettingsWeatherTimeoutFeedback = "天气同步超时";
constexpr const char *kSettingsSayingTimeoutFeedback = "一言更新超时";
static_assert(sizeof(s_settings_feedback_text) == kSettingsFeedbackTextLen,
              "settings feedback storage must match the public text contract");
static_assert(kSettingsSyncNone == 0, "settings sync state default must mean idle");

uint32_t advance_settings_sync_generation()
{
    ++s_settings_sync_generation;
    if (s_settings_sync_generation == 0) {
        ++s_settings_sync_generation;
    }
    return s_settings_sync_generation;
}

SettingsSyncRequestSnapshot settings_sync_state_load()
{
    SettingsSyncRequestSnapshot snapshot;
    ScopedSemaphoreLock lock(s_settings_sync_mutex.handle());
    if (!lock) {
        return snapshot;
    }
    return s_settings_sync_state;
}

void settings_sync_state_begin(SettingsSyncOp operation,
                               TickType_t deadline_tick,
                               EventBits_t request_bit)
{
    ScopedSemaphoreLock lock(s_settings_sync_mutex.handle());
    if (!lock) {
        return;
    }
    s_settings_sync_state.operation = operation;
    s_settings_sync_state.deadline_tick = deadline_tick;
    s_settings_sync_state.generation =
        advance_settings_sync_generation();
    s_settings_sync_state.request_bit = request_bit;
    s_settings_sync_state.request_generation =
        publish_network_sync_request(request_bit);
}

bool settings_sync_state_clear_if(SettingsSyncOp operation,
                                  uint32_t generation)
{
    ScopedSemaphoreLock lock(s_settings_sync_mutex.handle());
    if (!lock) {
        return false;
    }
    const bool matched =
        generation != 0 &&
        s_settings_sync_state.operation == operation &&
        s_settings_sync_state.generation == generation;
    if (matched) {
        s_settings_sync_state = {};
    }
    return matched;
}

void cancel_timed_out_manual_sync(
    const SettingsSyncRequestSnapshot &state,
    const char *feedback)
{
    if (retire_network_sync_request(state.request_bit,
                                    state.request_generation)) {
        notify_network_sync_runtime_state_changed();
    }
    finish_settings_sync(state.operation, state.generation, feedback);
}
} // namespace

bool settings_feedback_state_init()
{
    if (!s_settings_sync_mutex.init()) {
        return false;
    }
    return s_settings_feedback_mutex.init();
}

void set_settings_feedback(const char *text, uint32_t duration_ms)
{
    char next_feedback[kSettingsFeedbackTextLen] = {};
    ui_text::copy(next_feedback, sizeof(next_feedback), text);
    TickType_t now = xTaskGetTickCount();
    TickType_t until_tick = now + pdMS_TO_TICKS(duration_ms);
    {
        ScopedSemaphoreLock lock(s_settings_feedback_mutex.handle());
        if (!lock) {
            return;
        }
        memcpy(s_settings_feedback_text,
               next_feedback,
               sizeof(s_settings_feedback_text));
        s_settings_feedback_until_tick = until_tick;
    }
    if (settings_page_requested()) {
        settings_activity_record(now);
    }
    notify_ui_task();
}

void clear_settings_feedback()
{
    ScopedSemaphoreLock lock(s_settings_feedback_mutex.handle());
    if (!lock) {
        return;
    }
    s_settings_feedback_text[0] = '\0';
    s_settings_feedback_until_tick = 0;
}

bool settings_feedback_snapshot_load(TickType_t now,
                                     SettingsFeedbackSnapshot *out)
{
    if (!out) {
        return false;
    }
    SettingsFeedbackSnapshot snapshot;
    {
        ScopedSemaphoreLock lock(s_settings_feedback_mutex.handle());
        if (!lock) {
            return false;
        }
        snapshot.active =
            s_settings_feedback_text[0] != '\0' &&
            s_settings_feedback_until_tick != 0 &&
            app_tick_deadline_pending(now, s_settings_feedback_until_tick);
        if (snapshot.active) {
            memcpy(snapshot.text,
                   s_settings_feedback_text,
                   sizeof(snapshot.text));
        } else {
            s_settings_feedback_text[0] = '\0';
            s_settings_feedback_until_tick = 0;
        }
    }
    *out = snapshot;
    return true;
}

SettingsUiTimingSnapshot settings_ui_timing_snapshot_load()
{
    SettingsUiTimingSnapshot snapshot;
    {
        ScopedSemaphoreLock lock(s_settings_feedback_mutex.handle());
        if (lock) {
            snapshot.feedback_until_tick = s_settings_feedback_until_tick;
        }
    }
    const SettingsSyncRequestSnapshot sync_state =
        settings_sync_state_load();
    snapshot.sync_deadline_tick = sync_state.deadline_tick;
    snapshot.sync_busy = sync_state.operation != kSettingsSyncNone ||
                         network_diag_state_load() == kNetworkDiagRunning;
    return snapshot;
}

SettingsSyncRequestSnapshot settings_sync_request_snapshot_load()
{
    return settings_sync_state_load();
}

bool is_settings_sync_busy()
{
    const SettingsSyncRequestSnapshot state =
        settings_sync_state_load();
    return state.operation != kSettingsSyncNone ||
           network_diag_state_load() == kNetworkDiagRunning;
}

void begin_settings_sync(SettingsSyncOp op,
                         const char *text,
                         EventBits_t request_bit)
{
    TickType_t now = xTaskGetTickCount();
    settings_sync_state_begin(
        op,
        now + pdMS_TO_TICKS(kSettingsManualSyncTimeoutMs),
        request_bit);
    settings_activity_record(now);
    set_settings_feedback(text, kSettingsManualSyncTimeoutMs);
}

void finish_settings_sync(SettingsSyncOp op,
                          uint32_t generation,
                          const char *text)
{
    if (!settings_sync_state_clear_if(op, generation)) {
        return;
    }
    TickType_t now = xTaskGetTickCount();
    settings_activity_record(now);
    set_settings_feedback(text, 3500);
}

bool finish_settings_sync_if_timed_out(TickType_t now)
{
    const SettingsSyncRequestSnapshot state =
        settings_sync_state_load();
    bool busy = state.operation != kSettingsSyncNone ||
                network_diag_state_load() == kNetworkDiagRunning;
    if (!busy || state.deadline_tick == 0 ||
        !app_tick_deadline_reached(now, state.deadline_tick)) {
        return false;
    }

    const SettingsSyncOp op = state.operation;
    ESP_LOGW(TAG, SETTINGS_MANUAL_SYNC_TIMEOUT_LOG_FORMAT, op);
    if (op == kSettingsSyncNtp) {
        cancel_timed_out_manual_sync(state,
                                     kSettingsNtpTimeoutFeedback);
    } else if (op == kSettingsSyncWeather) {
        cancel_timed_out_manual_sync(state,
                                     kSettingsWeatherTimeoutFeedback);
    } else if (op == kSettingsSyncSaying) {
        cancel_timed_out_manual_sync(state,
                                     kSettingsSayingTimeoutFeedback);
    } else {
        settings_sync_state_clear_if(op, state.generation);
    }
    return true;
}
