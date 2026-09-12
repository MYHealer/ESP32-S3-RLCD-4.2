// 验证 housekeeping 在普通采样和 OTA 暂停期间的截止时间等待策略。
#include "housekeeping_wait_policy.h"

#include <cassert>
#include <cstdint>

int main()
{
    using Tick = uint32_t;
    constexpr Tick kFallback = 1000;
    constexpr Tick kIndefinite = UINT32_MAX;

    assert(housekeeping_wait_ticks<Tick>(
               false, false, 100, 0, 600, kFallback, kIndefinite) == 500);
    assert(housekeeping_wait_ticks<Tick>(
               false, false, 600, 0, 600, kFallback, kIndefinite) == kFallback);

    assert(housekeeping_wait_ticks<Tick>(
               true, false, 100, 0, 600, kFallback, kIndefinite) == kIndefinite);
    assert(housekeeping_wait_ticks<Tick>(
               true, true, 100, 350, 600, kFallback, kIndefinite) == 250);
    assert(housekeeping_wait_ticks<Tick>(
               true, true, 350, 350, 600, kFallback, kIndefinite) == kFallback);

    constexpr Tick kNearWrap = UINT32_MAX - 50;
    assert(housekeeping_wait_ticks<Tick>(
               false, false, kNearWrap, 0, 25, kFallback, kIndefinite) == 76);
    assert(housekeeping_wait_ticks<Tick>(
               true, true, kNearWrap, 10, 25, kFallback, kIndefinite) == 61);

    return 0;
}
