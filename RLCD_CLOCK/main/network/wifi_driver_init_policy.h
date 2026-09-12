// 定义 Wi-Fi 驱动初始化成功、安全可重试和禁止重试三种生命周期状态。
#pragma once

#include <stdint.h>

enum class WifiDriverInitState : uint8_t {
    kRetryable,
    kReady,
    kRetryBlocked,
};

constexpr bool wifi_driver_init_ready(WifiDriverInitState state)
{
    return state == WifiDriverInitState::kReady;
}

constexpr bool wifi_driver_init_retry_allowed(WifiDriverInitState state)
{
    return state == WifiDriverInitState::kRetryable;
}

constexpr WifiDriverInitState wifi_driver_init_state_after_attempt(
    bool succeeded,
    bool retry_safe)
{
    if (succeeded) {
        return WifiDriverInitState::kReady;
    }
    return retry_safe ? WifiDriverInitState::kRetryable
                      : WifiDriverInitState::kRetryBlocked;
}
