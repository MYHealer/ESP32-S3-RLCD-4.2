// 验证任意页面在无按压且 GPIO 唤醒可用时进入事件等待。
#include "input_button_wait_policy.h"

#include <assert.h>

int main()
{
    assert(button_task_can_wait_for_edge(true, false, false, false));
    assert(!button_task_can_wait_for_edge(false, false, false, false));
    assert(!button_task_can_wait_for_edge(true, true, false, true));
    assert(!button_task_can_wait_for_edge(true, false, true, true));
    assert(!button_task_can_wait_for_edge(true, false, false, true));

    assert(button_task_poll_delay_ms(true, false, false) ==
           kButtonPressedPollMs);
    assert(button_task_poll_delay_ms(false, true, false) ==
           kButtonActivePollMs);
    assert(button_task_poll_delay_ms(false, false, true) ==
           kButtonLowRefreshIdlePollMs);
    assert(button_task_poll_delay_ms(false, false, false) ==
           kButtonIdlePollMs);

    static_assert(kButtonPressedPollMs == 20);
    static_assert(kButtonActivePollMs == 50);
    static_assert(kButtonIdlePollMs == 250);
    static_assert(kButtonLowRefreshIdlePollMs == 500);

    assert(button_gpio_config_retry_due(1, false));
    assert(button_gpio_config_retry_due(2, false));
    assert(!button_gpio_config_retry_due(3, false));
    assert(!button_gpio_config_retry_due(1, true));
    static_assert(kButtonGpioConfigMaxAttempts == 3);
    static_assert(kButtonGpioConfigRetryDelayMs == 100);
    return 0;
}
