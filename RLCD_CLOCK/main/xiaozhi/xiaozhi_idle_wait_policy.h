// 定义小智稳定待唤醒状态下协调任务的事件等待策略。
#pragma once

#include "xiaozhi_ai.h"

constexpr bool xiaozhi_ai_idle_wait_until_event(XiaozhiAiState state,
                                                 bool voice_listening)
{
    return state == kXiaozhiAiReady && voice_listening;
}

constexpr bool xiaozhi_ai_configuration_blocked(bool offline_mode,
                                                bool have_wifi_credentials)
{
    return offline_mode || !have_wifi_credentials;
}
