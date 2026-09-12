// 计算充电动画可见性和整秒闪烁相位，不访问 UI 或共享全局状态。
#include "ui_battery_blink.h"

UiBatteryBlinkState ui_battery_blink_state(const UiBatteryBlinkInput &input)
{
    bool visible = input.charging &&
                   !input.animation_complete &&
                   input.percent >= 0 &&
                   input.percent < input.animation_stop_percent &&
                   input.active_page >= 0 &&
                   input.active_page < input.page_count &&
                   !input.setup_active &&
                   !input.auxiliary_page_active;
    bool second_even = input.time_plausible
                           ? input.local_second % 2 == 0
                           : input.fallback_elapsed_seconds % 2U == 0;
    bool on = visible && second_even;
    return {visible, on, on ? 1 : 0};
}
