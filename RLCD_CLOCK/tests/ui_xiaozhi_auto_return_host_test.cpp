// 验证小智自动返回的活动重置、超时和 Tick 回绕规则。
#include "ui_xiaozhi_auto_return.h"

#include <assert.h>
#include <stdint.h>

#include <limits>

int main()
{
    constexpr uint32_t timeout = 300;
    XiaozhiAutoReturnDecision decision =
        xiaozhi_auto_return_decision<uint32_t>(100, 0, timeout, true, false, false, false);
    assert(decision.record_activity && !decision.return_home);

    decision = xiaozhi_auto_return_decision<uint32_t>(100, 50, timeout, true, true, false, false);
    assert(decision.record_activity && !decision.return_home);
    decision = xiaozhi_auto_return_decision<uint32_t>(100, 50, timeout, true, false, true, false);
    assert(decision.record_activity && !decision.return_home);
    decision = xiaozhi_auto_return_decision<uint32_t>(100, 50, timeout, true, false, false, true);
    assert(decision.record_activity && !decision.return_home);

    decision = xiaozhi_auto_return_decision<uint32_t>(349, 50, timeout, true, false, false, false);
    assert(!decision.record_activity && !decision.return_home);
    decision = xiaozhi_auto_return_decision<uint32_t>(350, 50, timeout, true, false, false, false);
    assert(!decision.record_activity && decision.return_home);
    decision = xiaozhi_auto_return_decision<uint32_t>(350, 50, timeout, false, false, false, false);
    assert(!decision.record_activity && !decision.return_home);

    constexpr uint32_t tick_max = std::numeric_limits<uint32_t>::max();
    decision = xiaozhi_auto_return_decision<uint32_t>(4, tick_max - 5, 10, true, false, false, false);
    assert(!decision.record_activity && decision.return_home);

    return 0;
}
