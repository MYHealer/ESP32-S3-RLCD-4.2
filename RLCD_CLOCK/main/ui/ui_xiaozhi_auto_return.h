// 提供小智页面活动计时重置与自动返回到期的纯判断。
#pragma once

#include "app_tick_time.h"

struct XiaozhiAutoReturnDecision {
    bool record_activity = false;
    bool return_home = false;
};

template <typename Tick>
constexpr XiaozhiAutoReturnDecision xiaozhi_auto_return_decision(
    Tick now,
    Tick last_activity,
    Tick timeout,
    bool auto_return_enabled,
    bool pomodoro_running,
    bool conversation_active,
    bool activity_sequence_changed)
{
    if (pomodoro_running ||
        last_activity == 0 ||
        activity_sequence_changed ||
        conversation_active) {
        return {true, false};
    }
    if (auto_return_enabled && app_tick_interval_elapsed(now, last_activity, timeout)) {
        return {false, true};
    }
    return {};
}
