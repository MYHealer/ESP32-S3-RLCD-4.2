// 声明工作页状态栏事件刷新与低频兜底判断的纯逻辑接口。
#pragma once

#include <stdint.h>

inline constexpr uint32_t kUiStatusFallbackRefreshMs = 60U * 1000U;

struct UiStatusRefreshSnapshot {
    uint32_t sensor_version = 0;
    uint32_t weather_network_bits = 0;
    bool chime_enabled = false;
    bool wifi_radio_on = false;
    bool alarm_enabled = false;
};

bool ui_weather_network_status_required(bool weather_clock_visible,
                                        bool low_battery_mode,
                                        bool setup_portal_active);
bool ui_status_refresh_inputs_changed(const UiStatusRefreshSnapshot &current,
                                      const UiStatusRefreshSnapshot &previous);
bool ui_status_refresh_due(const UiStatusRefreshSnapshot &current,
                           const UiStatusRefreshSnapshot &previous,
                           bool previous_valid,
                           bool fallback_elapsed);
bool ui_sensor_status_refresh_due(const UiStatusRefreshSnapshot &current,
                                  const UiStatusRefreshSnapshot &previous,
                                  bool previous_valid,
                                  bool force_refresh);
