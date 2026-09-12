// 验证充电图标闪烁的可见条件、阈值和可信/不可信时间相位。
#include "ui_battery_blink.h"

#include <assert.h>

namespace {
constexpr int kStopPercent = 96;
constexpr int kPageCount = 7;

UiBatteryBlinkInput default_input()
{
    return {true, false, 50, kStopPercent, 0, kPageCount, false, false, true, 2, 0};
}
} // namespace

int main()
{
    UiBatteryBlinkInput input = default_input();
    UiBatteryBlinkState state = ui_battery_blink_state(input);
    assert(state.visible && state.on && state.phase == 1);

    input.local_second = 3;
    state = ui_battery_blink_state(input);
    assert(state.visible && !state.on && state.phase == 0);

    input = default_input();
    input.charging = false;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.animation_complete = true;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.percent = -1;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.percent = kStopPercent;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.percent = kStopPercent - 1;
    assert(ui_battery_blink_state(input).visible);

    input = default_input();
    input.active_page = -1;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.active_page = kPageCount;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.setup_active = true;
    assert(!ui_battery_blink_state(input).visible);
    input = default_input();
    input.auxiliary_page_active = true;
    assert(!ui_battery_blink_state(input).visible);

    input = default_input();
    input.time_plausible = false;
    input.local_second = 3;
    input.fallback_elapsed_seconds = 8;
    state = ui_battery_blink_state(input);
    assert(state.visible && state.on && state.phase == 1);
    input.fallback_elapsed_seconds = 9;
    state = ui_battery_blink_state(input);
    assert(state.visible && !state.on && state.phase == 0);
    return 0;
}
