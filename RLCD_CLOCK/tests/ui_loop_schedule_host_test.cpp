// 验证 UI 主循环秒/分钟边界和最短轮询候选选择规则。
#include "ui_loop_schedule.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>

int main()
{
    assert(ui_next_second_delay_ms(0, 0) == 1005);
    assert(ui_next_second_delay_ms(0, 500000) == 505);
    assert(ui_next_second_delay_ms(0, 999000) == 10);
    assert(ui_next_second_delay_ms(0, 999999) == 10);
    assert(ui_next_second_delay_ms(1, 1000000) == 1005);
    assert(ui_next_second_delay_ms(1700000000LL, 1700000000500000LL) == 505);
    assert(ui_next_second_delay_ms(0, 1000000) == 10);
    assert(ui_next_second_delay_ms(0, -1) == 1005);

    assert(ui_local_time_cache_refresh_due(100, 100, false));
    assert(!ui_local_time_cache_refresh_due(100, 100, true));
    assert(ui_local_time_cache_refresh_due(101, 100, true));
    assert(ui_local_time_cache_refresh_due(99, 100, true));

    assert(ui_low_battery_minute_idle(true, false));
    assert(!ui_low_battery_minute_idle(false, false));
    assert(!ui_low_battery_minute_idle(true, true));

    assert(ui_low_refresh_page_minute_idle(true, false, false, false));
    assert(!ui_low_refresh_page_minute_idle(false, false, false, false));
    assert(!ui_low_refresh_page_minute_idle(true, true, false, false));
    assert(!ui_low_refresh_page_minute_idle(true, false, true, false));
    assert(!ui_low_refresh_page_minute_idle(true, false, false, true));

    assert(ui_xiaozhi_activation_update_due(false, false, false, true));
    assert(!ui_xiaozhi_activation_update_due(false, false, true, true));
    assert(ui_xiaozhi_activation_update_due(true, false, true, false));
    assert(ui_xiaozhi_activation_update_due(true, true, true, true));
    assert(!ui_xiaozhi_activation_update_due(true, true, true, false));
    assert(ui_xiaozhi_activation_update_due(false, true, true, false));

    assert(ui_next_minute_delay_ms(0, 0) == 60005);
    assert(ui_next_minute_delay_ms(1, 1000000) == 59005);
    assert(ui_next_minute_delay_ms(1, 1500000) == 58505);
    assert(ui_next_minute_delay_ms(59, 59999000) == 10);
    assert(ui_next_minute_delay_ms(60, 60000000) == 60005);
    assert(ui_next_minute_delay_ms(0, 1000000) == 10);
    assert(ui_next_minute_delay_ms(0, -1) == 10);

    assert(ui_pomodoro_boundary_delay_ms(0) == 0);
    assert(ui_pomodoro_boundary_delay_ms(995) == 1000);
    assert(ui_lvgl_lock_retry_delay_ms(0) == 0);
    assert(ui_lvgl_lock_retry_delay_ms(1) == 100);
    assert(ui_lvgl_lock_retry_delay_ms(2) == 200);
    assert(ui_lvgl_lock_retry_delay_ms(3) == 400);
    assert(ui_lvgl_lock_retry_delay_ms(4) == 800);
    assert(ui_lvgl_lock_retry_delay_ms(5) == 1000);
    assert(ui_lvgl_lock_retry_delay_ms(255) == 1000);

    const uint32_t candidates[] = {250, 0, 50, 75, 0};
    assert(ui_shortest_delay_ticks(candidates, 5) == 50);
    const uint32_t leading_zero[] = {0, 100, 25};
    assert(ui_shortest_delay_ticks(leading_zero, 3) == 25);
    const uint32_t all_zero[] = {0, 0, 0};
    assert(ui_shortest_delay_ticks(all_zero, 3) == 0);
    assert(ui_shortest_delay_ticks(nullptr, 3) == 0);
    assert(ui_shortest_delay_ticks(candidates, 0) == 0);

    assert(ui_inactivity_wait_ticks(100, 100, 30000) == 30000);
    assert(ui_inactivity_wait_ticks(30100, 100, 30000) == 1);
    assert(ui_inactivity_wait_ticks(UINT32_MAX - 5, UINT32_MAX - 15, 20) == 10);

    UiInfoPageWaitInput info = {};
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == UINT32_MAX);
    info.now_tick = 100;
    info.hold_until_tick = 30100;
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == 30000);
    info.now_tick = 30100;
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == 1);
    info.ota_flow_active = true;
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == 1000);
    info.ota_updating = true;
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == 3000);
    info.ota_updating = false;
    info.ota_flow_active = false;
    info.now_tick = UINT32_MAX - 5;
    info.hold_until_tick = 4;
    assert(ui_info_page_wait_ticks(info, 1000, 3000) == 10);

    UiSettingsWaitInput settings = {};
    settings.now_tick = 100;
    settings.last_activity_tick = 100;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 30000);

    settings.now_tick = 0;
    settings.last_activity_tick = 0;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 30000);
    settings.now_tick = 100;
    settings.last_activity_tick = 100;

    settings.feedback_until_tick = 2600;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 2500);
    settings.feedback_until_tick = 0;
    settings.sync_busy = true;
    settings.sync_deadline_tick = 60100;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 60000);

    settings.sync_deadline_tick = 100;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 1);
    settings.sync_busy = false;
    settings.sync_deadline_tick = 0;
    settings.ota_flow_active = true;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == UINT32_MAX);

    settings.ota_updating = true;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 3000);
    settings.ota_updating = false;
    settings.ota_status_hold_set = true;
    settings.ota_status_until_tick = 900;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 800);

    settings = {};
    settings.now_tick = UINT32_MAX - 5;
    settings.feedback_until_tick = 4;
    assert(ui_settings_wait_ticks(settings, 30000, 3000) == 10);
    return 0;
}
