// 集中声明 BOOT/KEY 硬件引脚和输入任务轮询参数。
#pragma once

#include "driver/gpio.h"

inline constexpr gpio_num_t kBootButtonGpio = GPIO_NUM_0;
inline constexpr gpio_num_t kKeyButtonGpio = GPIO_NUM_18;

inline constexpr int kButtonIdlePollMs = 250;
inline constexpr int kButtonLowRefreshIdlePollMs = 500;
inline constexpr int kButtonActivePollMs = 50;
inline constexpr int kButtonPressedPollMs = 20;
