// 使用静态任务互斥发布番茄钟状态与 64 位单调时间，避免跨核关中断。
#include "pomodoro_runtime_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <atomic>

namespace {
StaticTaskMutex s_runtime_mutex;
PomodoroSnapshot s_visible = {kPomodoroIdle, 0, 0, false, 1};
int64_t s_deadline_us = 0;
int64_t s_completed_at_us = 0;
std::atomic<PomodoroState> s_state{kPomodoroIdle};

uint32_t remaining_ms(int64_t now_us)
{
    if (s_visible.state != kPomodoroRunning || s_deadline_us <= now_us) {
        return 0;
    }
    const int64_t remaining_us = s_deadline_us - now_us;
    return static_cast<uint32_t>((remaining_us + 999) / 1000);
}
}

bool pomodoro_runtime_state_init()
{
    if (!s_runtime_mutex.init()) {
        return false;
    }
    s_state.store(s_visible.state, std::memory_order_release);
    return true;
}

bool pomodoro_runtime_publish(PomodoroState state,
                              uint32_t total_ms,
                              uint32_t remaining,
                              bool alerting,
                              int64_t deadline_us,
                              int64_t completed_at_us)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    s_visible.state = state;
    s_visible.total_ms = total_ms;
    s_visible.remaining_ms = remaining;
    s_visible.alerting = alerting;
    ++s_visible.version;
    s_deadline_us = deadline_us;
    s_completed_at_us = completed_at_us;
    s_state.store(state, std::memory_order_release);
    return true;
}

bool pomodoro_runtime_snapshot(int64_t now_us, PomodoroRuntimeSnapshot *out)
{
    if (!out) {
        return false;
    }
    *out = {};
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    out->visible = s_visible;
    out->visible.remaining_ms = remaining_ms(now_us);
    out->completed_at_us = s_completed_at_us;
    return true;
}

bool pomodoro_runtime_set_alerting(bool alerting)
{
    ScopedSemaphoreLock lock(s_runtime_mutex);
    if (!lock) {
        return false;
    }
    if (s_visible.alerting != alerting) {
        s_visible.alerting = alerting;
        ++s_visible.version;
    }
    return true;
}

PomodoroState pomodoro_runtime_state_load()
{
    return s_state.load(std::memory_order_acquire);
}
