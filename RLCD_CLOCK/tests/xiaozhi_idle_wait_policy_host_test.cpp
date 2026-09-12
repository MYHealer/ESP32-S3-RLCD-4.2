// 验证小智仅在稳定 Ready 且 WakeNet 正常监听时进入事件等待。
#include "xiaozhi_idle_wait_policy.h"

#include <assert.h>

int main()
{
    assert(xiaozhi_ai_idle_wait_until_event(kXiaozhiAiReady, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiReady, false));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiInactive, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiWaitingForWifi, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiActivating, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiBinding, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiListening, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiSpeaking, true));
    assert(!xiaozhi_ai_idle_wait_until_event(kXiaozhiAiError, true));

    assert(!xiaozhi_ai_configuration_blocked(false, true));
    assert(xiaozhi_ai_configuration_blocked(true, true));
    assert(xiaozhi_ai_configuration_blocked(false, false));
    assert(xiaozhi_ai_configuration_blocked(true, false));
    return 0;
}
