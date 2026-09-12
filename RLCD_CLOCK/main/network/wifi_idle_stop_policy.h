// 判断延后的 Wi-Fi 关闭请求何时可以安全执行。
#pragma once

#include <stdint.h>

struct WifiIdleStopPolicyInput {
    bool requested = false;
    bool radio_on = false;
    bool setup_portal_active = false;
    bool ota_active = false;
    bool xiaozhi_keepalive_active = false;
    bool network_lock_active = false;
};

constexpr bool wifi_idle_stop_allowed(const WifiIdleStopPolicyInput &input)
{
    return input.requested &&
           input.radio_on &&
           !input.setup_portal_active &&
           !input.ota_active &&
           !input.xiaozhi_keepalive_active &&
           !input.network_lock_active;
}

constexpr bool wifi_owned_normal_stop_allowed(bool lock_depth_available,
                                              int network_lock_depth)
{
    return lock_depth_available &&
           network_lock_depth >= 0 &&
           network_lock_depth <= 1;
}

constexpr uint32_t wifi_idle_stop_retry_delay_ms(uint8_t failure_count)
{
    if (failure_count == 0) {
        return 0;
    }
    constexpr uint32_t kInitialDelayMs = 1000;
    constexpr uint32_t kMaximumDelayMs = 60000;
    uint32_t delay_ms = kInitialDelayMs;
    for (uint8_t failure = 1;
         failure < failure_count && delay_ms < kMaximumDelayMs;
         ++failure) {
        delay_ms = delay_ms > kMaximumDelayMs / 2
                       ? kMaximumDelayMs
                       : delay_ms * 2;
    }
    return delay_ms;
}
