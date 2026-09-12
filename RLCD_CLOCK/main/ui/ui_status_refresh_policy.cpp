// 实现工作页状态栏按变化刷新策略，不访问 LVGL、任务或硬件状态。
#include "ui_status_refresh_policy.h"

static_assert(kUiStatusFallbackRefreshMs > 0,
              "UI status fallback refresh interval must be positive");

bool ui_weather_network_status_required(bool weather_clock_visible,
                                        bool low_battery_mode,
                                        bool setup_portal_active)
{
    return weather_clock_visible &&
           !low_battery_mode &&
           !setup_portal_active;
}

bool ui_status_refresh_inputs_changed(const UiStatusRefreshSnapshot &current,
                                      const UiStatusRefreshSnapshot &previous)
{
    return current.sensor_version != previous.sensor_version ||
           current.weather_network_bits != previous.weather_network_bits ||
           current.chime_enabled != previous.chime_enabled ||
           current.wifi_radio_on != previous.wifi_radio_on ||
           current.alarm_enabled != previous.alarm_enabled;
}

bool ui_status_refresh_due(const UiStatusRefreshSnapshot &current,
                           const UiStatusRefreshSnapshot &previous,
                           bool previous_valid,
                           bool fallback_elapsed)
{
    return !previous_valid ||
           fallback_elapsed ||
           ui_status_refresh_inputs_changed(current, previous);
}

bool ui_sensor_status_refresh_due(const UiStatusRefreshSnapshot &current,
                                  const UiStatusRefreshSnapshot &previous,
                                  bool previous_valid,
                                  bool force_refresh)
{
    return force_refresh ||
           !previous_valid ||
           current.sensor_version != previous.sensor_version;
}
