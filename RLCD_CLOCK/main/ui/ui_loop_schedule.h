// 声明 UI 主循环秒/分钟边界和最短轮询延迟的纯计算接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr uint32_t kUiLoopBoundaryWakeSlackMs = 5;

struct UiSettingsWaitInput {
    uint32_t now_tick = 0;
    uint32_t last_activity_tick = 0;
    uint32_t feedback_until_tick = 0;
    uint32_t sync_deadline_tick = 0;
    uint32_t ota_status_until_tick = 0;
    bool sync_busy = false;
    bool ota_flow_active = false;
    bool ota_updating = false;
    bool ota_status_hold_set = false;
};

struct UiInfoPageWaitInput {
    uint32_t now_tick = 0;
    uint32_t hold_until_tick = 0;
    bool ota_flow_active = false;
    bool ota_updating = false;
};

uint32_t ui_next_second_delay_ms(int64_t sampled_wall_second,
                                 int64_t wall_clock_us);
bool ui_local_time_cache_refresh_due(int64_t sampled_wall_second,
                                     int64_t cached_wall_second,
                                     bool cache_valid);
bool ui_low_battery_minute_idle(bool low_battery_mode,
                                bool auxiliary_page_requested);
bool ui_low_refresh_page_minute_idle(bool page_uses_low_refresh,
                                     bool low_battery_mode,
                                     bool setup_portal_active,
                                     bool auxiliary_page_requested);
bool ui_xiaozhi_activation_update_due(bool requested_active,
                                      bool previous_requested_active,
                                      bool previous_valid,
                                      bool allow_active_retry);
uint32_t ui_next_minute_delay_ms(int64_t sampled_wall_second,
                                 int64_t wall_clock_us);
uint32_t ui_pomodoro_boundary_delay_ms(uint32_t boundary_ms);
uint32_t ui_lvgl_lock_retry_delay_ms(uint8_t consecutive_failures);
uint32_t ui_shortest_delay_ticks(const uint32_t *candidates, size_t count);
uint32_t ui_inactivity_wait_ticks(uint32_t now_tick,
                                  uint32_t last_activity_tick,
                                  uint32_t timeout_ticks);
uint32_t ui_info_page_wait_ticks(const UiInfoPageWaitInput &input,
                                 uint32_t ota_active_fallback_ticks,
                                 uint32_t ota_updating_fallback_ticks);
uint32_t ui_settings_wait_ticks(const UiSettingsWaitInput &input,
                                uint32_t settings_timeout_ticks,
                                uint32_t ota_updating_fallback_ticks);
