// 实现天气时钟预警条轮播规则，不访问 LVGL、天气快照或全局状态。
#include "ui_clock_alert_state.h"

namespace {

constexpr int kAlertVisibilityPeriodSeconds = 2;

} // namespace

static_assert(kAlertVisibilityPeriodSeconds > 0,
              "clock alert visibility period must be positive");

ClockAlertDisplayState clock_alert_display_state(int second,
                                                 bool low_battery_mode,
                                                 bool alert_active,
                                                 int alert_count)
{
    ClockAlertDisplayState state = {};
    state.visible = !low_battery_mode && alert_active && alert_count > 0 &&
                    second % kAlertVisibilityPeriodSeconds == 0;
    state.index = alert_count > 0
                      ? (second / kAlertVisibilityPeriodSeconds) % alert_count
                      : 0;
    return state;
}

bool clock_alert_display_needs_update(const ClockAlertDisplayState &next,
                                      bool current_visible,
                                      int current_index,
                                      bool status_due)
{
    return next.visible != current_visible ||
           (next.visible && next.index != current_index) ||
           (next.visible && status_due);
}
