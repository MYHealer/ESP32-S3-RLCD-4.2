// 计算 housekeeping 在普通采样或 OTA 暂停状态下的通知等待时间。
#pragma once

#include "app_tick_time.h"

template <typename Tick>
constexpr Tick housekeeping_wait_ticks(bool ota_active,
                                       bool ota_hold_set,
                                       Tick now,
                                       Tick ota_hold_deadline,
                                       Tick sample_deadline,
                                       Tick fallback_wait,
                                       Tick indefinite_wait)
{
    if (ota_active && !ota_hold_set) {
        return indefinite_wait;
    }
    Tick deadline = ota_active ? ota_hold_deadline : sample_deadline;
    Tick remaining = app_tick_deadline_remaining(now, deadline);
    return remaining > 0 ? remaining : fallback_wait;
}
