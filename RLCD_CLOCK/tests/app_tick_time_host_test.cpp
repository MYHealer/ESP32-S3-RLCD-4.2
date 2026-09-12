// 验证短时 Tick 间隔和截止时间在普通及回绕边界的判断结果。
#include "app_tick_time.h"

#include <assert.h>
#include <stdint.h>

#include <limits>

int main()
{
    constexpr uint32_t tick_max = std::numeric_limits<uint32_t>::max();

    static_assert(!app_tick_interval_elapsed<uint32_t>(109, 100, 10));
    static_assert(app_tick_interval_elapsed<uint32_t>(110, 100, 10));
    static_assert(!app_tick_interval_elapsed<uint32_t>(9, 0, 10));
    static_assert(app_tick_interval_elapsed<uint32_t>(10, 0, 10));
    static_assert(!app_tick_interval_elapsed<uint32_t>(3, tick_max - 5, 10));
    static_assert(app_tick_interval_elapsed<uint32_t>(4, tick_max - 5, 10));

    static_assert(app_tick_nonzero_delay<uint32_t>(0) == 1);
    static_assert(app_tick_nonzero_delay<uint32_t>(25) == 25);
    static_assert(app_tick_nonzero_delay<uint16_t>(0) == 1);
    static_assert(app_tick_nonzero_delay<uint32_t>(tick_max) == tick_max);

    static_assert(app_tick_deadline_pending<uint32_t>(100, 110));
    static_assert(app_tick_deadline_reached<uint32_t>(110, 110));
    static_assert(app_tick_deadline_reached<uint32_t>(111, 110));

    constexpr uint32_t wrapped_deadline = 4;
    static_assert(app_tick_deadline_pending<uint32_t>(tick_max - 1, wrapped_deadline));
    static_assert(app_tick_deadline_reached<uint32_t>(wrapped_deadline, wrapped_deadline));
    static_assert(app_tick_deadline_reached<uint32_t>(5, wrapped_deadline));
    static_assert(app_tick_deadline_pending<uint32_t>(tick_max, 0));

    static_assert(app_tick_deadline_remaining<uint32_t>(100, 110) == 10);
    static_assert(app_tick_deadline_remaining<uint32_t>(110, 110) == 0);
    static_assert(app_tick_deadline_remaining<uint32_t>(111, 110) == 0);
    static_assert(app_tick_deadline_remaining<uint32_t>(tick_max - 5, 4) == 10);
    static_assert(app_tick_deadline_remaining<uint32_t>(tick_max, 0) == 1);

    static_assert(app_tick_earlier_deadline<uint32_t>(100, 120, 130) == 120);
    static_assert(app_tick_earlier_deadline<uint32_t>(tick_max - 10, 5, tick_max - 1) ==
                  tick_max - 1);
    static_assert(app_tick_earlier_deadline<uint32_t>(tick_max - 10, tick_max - 1, 5) ==
                  tick_max - 1);

    assert(app_tick_interval_elapsed<uint32_t>(0, tick_max, 1));
    assert(app_tick_deadline_reached<uint32_t>(0, 0));
    return 0;
}
