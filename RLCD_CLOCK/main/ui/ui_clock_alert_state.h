// 声明天气时钟预警条秒级显隐与轮播索引的纯状态计算。
#pragma once

struct ClockAlertDisplayState {
    bool visible = false;
    int index = 0;
};

ClockAlertDisplayState clock_alert_display_state(int second,
                                                 bool low_battery_mode,
                                                 bool alert_active,
                                                 int alert_count);

bool clock_alert_display_needs_update(const ClockAlertDisplayState &next,
                                      bool current_visible,
                                      int current_index,
                                      bool status_due);
