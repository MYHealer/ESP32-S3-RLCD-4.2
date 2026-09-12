// 定义按键 GPIO 初始化重试和松开后事件等待的纯判断策略。
#pragma once

#include "input_button_config.h"

constexpr unsigned kButtonGpioConfigMaxAttempts = 3;
constexpr int kButtonGpioConfigRetryDelayMs = 100;

constexpr bool button_gpio_config_retry_due(unsigned completed_attempt,
                                            bool succeeded)
{
    return !succeeded && completed_attempt < kButtonGpioConfigMaxAttempts;
}

constexpr bool button_task_can_wait_for_edge(bool edge_wakeup_ready,
                                             bool boot_pressed,
                                             bool key_pressed,
                                             bool press_tracking_active)
{
    return edge_wakeup_ready &&
           !boot_pressed &&
           !key_pressed &&
           !press_tracking_active;
}

constexpr int button_task_poll_delay_ms(bool any_button_pressed,
                                        bool interactive_surface,
                                        bool low_refresh_surface)
{
    if (any_button_pressed) {
        return kButtonPressedPollMs;
    }
    if (interactive_surface) {
        return kButtonActivePollMs;
    }
    if (low_refresh_surface) {
        return kButtonLowRefreshIdlePollMs;
    }
    return kButtonIdlePollMs;
}
