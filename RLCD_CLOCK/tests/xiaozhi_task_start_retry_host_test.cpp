// 验证小智任务创建首次立即执行、失败退避、重置和 Tick 回绕。
#include "xiaozhi_task_start_retry.h"

#include <assert.h>
#include <stdint.h>

#include <limits>

int main()
{
    XiaozhiTaskStartRetryState<uint32_t> retry;
    constexpr uint32_t delay = 5000;

    assert(retry.attempt_due(100, delay));
    assert(!retry.waiting());

    retry.record_failure(100);
    assert(retry.waiting());
    assert(retry.failure_tick() == 100);
    assert(!retry.attempt_due(5099, delay));
    assert(retry.attempt_due(5100, delay));

    retry.record_failure(5100);
    assert(!retry.attempt_due(10099, delay));
    assert(retry.attempt_due(10100, delay));

    retry.reset();
    assert(!retry.waiting());
    assert(retry.failure_tick() == 0);
    assert(retry.attempt_due(10100, delay));

    constexpr uint32_t tick_max = std::numeric_limits<uint32_t>::max();
    retry.record_failure(tick_max - 1000U);
    assert(!retry.attempt_due(3998U, delay));
    assert(retry.attempt_due(3999U, delay));
    return 0;
}
