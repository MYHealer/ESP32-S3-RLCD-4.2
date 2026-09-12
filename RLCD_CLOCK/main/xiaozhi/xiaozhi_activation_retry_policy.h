// 判断小智激活配置是否需要首次加载或按既有周期重试。
#pragma once

#include "app_tick_time.h"
#include "xiaozhi_ai.h"

constexpr bool xiaozhi_activation_state_is_stable(XiaozhiAiState state)
{
    return state == kXiaozhiAiReady ||
           state == kXiaozhiAiListening ||
           state == kXiaozhiAiSpeaking;
}

template <typename Tick>
constexpr bool xiaozhi_activation_attempt_due(XiaozhiAiState state,
                                               bool attempt_scheduled,
                                               Tick now,
                                               Tick next_attempt)
{
    if (xiaozhi_activation_state_is_stable(state)) {
        return false;
    }
    return !attempt_scheduled || app_tick_deadline_reached(now, next_attempt);
}

template <typename Tick>
constexpr Tick xiaozhi_activation_retry_wait_ticks(XiaozhiAiState state,
                                                    bool attempt_scheduled,
                                                    Tick now,
                                                    Tick next_attempt,
                                                    Tick minimum_wait)
{
    if (xiaozhi_activation_state_is_stable(state) || !attempt_scheduled) {
        return minimum_wait;
    }
    Tick remaining = app_tick_deadline_remaining(now, next_attempt);
    return remaining > minimum_wait ? remaining : minimum_wait;
}
