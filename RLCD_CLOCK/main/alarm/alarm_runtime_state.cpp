// 使用静态任务互斥发布闹钟状态和覆盖确认，避免普通任务复制状态时关闭跨核中断。
#include "alarm_runtime_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <atomic>

namespace {
StaticTaskMutex s_runtime_mutex;
AlarmSnapshot s_alarm = {false, false, 0, 0};
AlarmReplacementConfirmation s_replacement_confirmation = {};
AlarmPendingSaveSnapshot s_pending_save = {};
std::atomic<bool> s_enabled{false};
uint32_t s_generation = 1;

void publish_locked(bool enabled, bool ringing, int hour, int minute)
{
    s_alarm.enabled = enabled;
    s_alarm.ringing = ringing;
    s_alarm.hour = static_cast<uint8_t>(hour);
    s_alarm.minute = static_cast<uint8_t>(minute);
    ++s_generation;
    if (s_generation == 0) {
        s_generation = 1;
    }
    s_enabled.store(enabled, std::memory_order_release);
}
}

bool alarm_runtime_state_init()
{
    if (!s_runtime_mutex.init()) {
        return false;
    }
    return true;
}

bool alarm_runtime_publish(bool enabled,
                           bool ringing,
                           int hour,
                           int minute)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    publish_locked(enabled, ringing, hour, minute);
    return true;
}

bool alarm_runtime_publish_deferred_save(bool enabled,
                                         bool ringing,
                                         int hour,
                                         int minute)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    publish_locked(enabled, ringing, hour, minute);
    s_pending_save.pending = true;
    s_pending_save.enabled = enabled;
    s_pending_save.hour = static_cast<uint8_t>(hour);
    s_pending_save.minute = static_cast<uint8_t>(minute);
    s_pending_save.generation = s_generation;
    return true;
}

bool alarm_runtime_snapshot(AlarmSnapshot *out)
{
    if (!out) {
        return false;
    }
    *out = {};
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    *out = s_alarm;
    return true;
}

bool alarm_runtime_is_enabled()
{
    return s_enabled.load(std::memory_order_acquire);
}

bool alarm_runtime_pending_save_snapshot(AlarmPendingSaveSnapshot *out)
{
    if (!out) {
        return false;
    }
    *out = {};
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    *out = s_pending_save;
    return true;
}

bool alarm_runtime_pending_save_exists()
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    return lock && s_pending_save.pending;
}

bool alarm_runtime_pending_save_clear(uint32_t generation)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock || !s_pending_save.pending ||
        generation != s_pending_save.generation) {
        return false;
    }
    s_pending_save = {};
    return true;
}

bool alarm_runtime_pending_save_discard()
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    s_pending_save = {};
    return true;
}

bool alarm_runtime_clear_replacement()
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    clear_alarm_replacement_confirmation(&s_replacement_confirmation);
    return true;
}

AlarmReplacementDecision alarm_runtime_replacement_decision(
    int requested_hour,
    int requested_minute,
    bool confirmed,
    uint32_t now_ms,
    uint32_t timeout_ms,
    AlarmSnapshot *existing)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        if (existing) {
            *existing = {};
        }
        return kAlarmReplacementConfirmationInvalid;
    }
    if (existing) {
        *existing = s_alarm;
    }
    return evaluate_alarm_replacement(s_alarm.enabled,
                                      s_alarm.hour,
                                      s_alarm.minute,
                                      s_generation,
                                      requested_hour,
                                      requested_minute,
                                      confirmed,
                                      now_ms,
                                      timeout_ms,
                                      &s_replacement_confirmation);
}
