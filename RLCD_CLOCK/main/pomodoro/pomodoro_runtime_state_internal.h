// 声明仅供番茄钟服务实现使用的复合运行态与单调时间接口。
#pragma once

#include "pomodoro_services.h"

#include <stdint.h>

struct PomodoroRuntimeSnapshot {
    PomodoroSnapshot visible;
    int64_t completed_at_us;
};

bool pomodoro_runtime_state_init();
bool pomodoro_runtime_publish(PomodoroState state,
                              uint32_t total_ms,
                              uint32_t remaining_ms,
                              bool alerting,
                              int64_t deadline_us,
                              int64_t completed_at_us);
bool pomodoro_runtime_snapshot(int64_t now_us, PomodoroRuntimeSnapshot *out);
bool pomodoro_runtime_set_alerting(bool alerting);
PomodoroState pomodoro_runtime_state_load();
