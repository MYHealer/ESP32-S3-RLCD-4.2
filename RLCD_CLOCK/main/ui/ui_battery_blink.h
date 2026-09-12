// 声明所有工作页共用的充电电池图标闪烁纯状态计算。
#pragma once

#include <stdint.h>

struct UiBatteryBlinkInput {
    bool charging;
    bool animation_complete;
    int percent;
    int animation_stop_percent;
    int active_page;
    int page_count;
    bool setup_active;
    bool auxiliary_page_active;
    bool time_plausible;
    int local_second;
    uint32_t fallback_elapsed_seconds;
};

struct UiBatteryBlinkState {
    bool visible;
    bool on;
    int phase;
};

UiBatteryBlinkState ui_battery_blink_state(const UiBatteryBlinkInput &input);
