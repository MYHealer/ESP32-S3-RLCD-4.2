// 管理小智语音番茄钟的单调计时、后台完成提醒和 UI 快照。
#pragma once

#include <stdint.h>

enum PomodoroState {
    kPomodoroIdle = 0,
    kPomodoroRunning,
    kPomodoroCompleted,
};

struct PomodoroSnapshot {
    PomodoroState state;
    uint32_t total_ms;
    uint32_t remaining_ms;
    bool alerting;
    uint32_t version;
};

constexpr uint32_t pomodoro_display_seconds(uint32_t remaining_ms)
{
    return remaining_ms == 0 ? 0 : 1U + (remaining_ms - 1U) / 1000U;
}

constexpr uint32_t pomodoro_next_display_boundary_ms(uint32_t remaining_ms)
{
    if (remaining_ms == 0) {
        return 0;
    }
    uint32_t remainder = remaining_ms % 1000U;
    return remainder == 0 ? 1000U : remainder;
}

void pomodoro_task(void *);
void pomodoro_get_snapshot(PomodoroSnapshot *out);
bool pomodoro_is_running();
bool pomodoro_start(uint32_t duration_seconds);
bool pomodoro_cancel();
bool pomodoro_stop_alert_from_button();
