// 验证小智稳定状态跳过激活 NVS 重读，异常状态保留周期重试和 Tick 回绕。
#include "xiaozhi_activation_retry_policy.h"

#include <assert.h>
#include <stdint.h>

#include <limits>

int main()
{
    constexpr uint32_t now = 100;
    constexpr uint32_t deadline = 200;

    assert(!xiaozhi_activation_attempt_due(kXiaozhiAiReady, false, now, deadline));
    assert(!xiaozhi_activation_attempt_due(kXiaozhiAiListening, true, 250U, deadline));
    assert(!xiaozhi_activation_attempt_due(kXiaozhiAiSpeaking, true, 250U, deadline));

    assert(xiaozhi_activation_attempt_due(kXiaozhiAiInactive, false, now, deadline));
    assert(!xiaozhi_activation_attempt_due(kXiaozhiAiBinding, true, now, deadline));
    assert(xiaozhi_activation_attempt_due(kXiaozhiAiBinding, true, deadline, deadline));
    assert(xiaozhi_activation_attempt_due(kXiaozhiAiError, true, 250U, deadline));

    constexpr uint32_t tick_max = std::numeric_limits<uint32_t>::max();
    assert(!xiaozhi_activation_attempt_due(kXiaozhiAiActivating,
                                            true,
                                            tick_max - 5U,
                                            4U));
    assert(xiaozhi_activation_attempt_due(kXiaozhiAiActivating, true, 4U, 4U));

    constexpr uint32_t minimum_wait = 5;
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiReady, true, 100U, 200U, minimum_wait) == minimum_wait);
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiError, false, 100U, 200U, minimum_wait) == minimum_wait);
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiError, true, 100U, 200U, minimum_wait) == 100U);
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiBinding, true, 198U, 200U, minimum_wait) == minimum_wait);
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiError, true, 200U, 200U, minimum_wait) == minimum_wait);
    assert(xiaozhi_activation_retry_wait_ticks(
               kXiaozhiAiError, true, tick_max - 5U, 4U, minimum_wait) == 10U);
    return 0;
}
