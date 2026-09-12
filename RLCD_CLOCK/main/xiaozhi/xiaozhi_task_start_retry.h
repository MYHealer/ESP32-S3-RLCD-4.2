// 提供小智主任务创建失败后的回绕安全退避状态。
#pragma once

#include "app_tick_time.h"

template <typename Tick>
class XiaozhiTaskStartRetryState {
public:
    constexpr bool attempt_due(Tick now, Tick retry_delay) const
    {
        return !failure_recorded_ ||
               app_tick_interval_elapsed(now, failure_tick_, retry_delay);
    }

    constexpr void record_failure(Tick now)
    {
        failure_recorded_ = true;
        failure_tick_ = now;
    }

    constexpr void reset()
    {
        failure_recorded_ = false;
        failure_tick_ = 0;
    }

    constexpr bool waiting() const { return failure_recorded_; }
    constexpr Tick failure_tick() const { return failure_tick_; }

private:
    bool failure_recorded_ = false;
    Tick failure_tick_ = 0;
};
