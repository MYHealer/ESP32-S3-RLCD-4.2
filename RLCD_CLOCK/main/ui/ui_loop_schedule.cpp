// 实现 UI 主循环轮询边界计算，不访问 LVGL、页面状态或硬件。
#include "ui_loop_schedule.h"

#include "app_tick_time.h"

#include <limits.h>

namespace {
constexpr int64_t kUsPerSecond = 1000000;
constexpr int64_t kUsPerMinute = 60 * kUsPerSecond;
constexpr uint32_t kMsPerSecond = 1000;
constexpr uint32_t kNextSecondDelayMinMs = 10;
constexpr uint32_t kNextSecondDelayMaxMs = kMsPerSecond + kUiLoopBoundaryWakeSlackMs;
constexpr uint32_t kNextMinuteDelayMaxMs =
    60 * kMsPerSecond + kUiLoopBoundaryWakeSlackMs;
constexpr uint32_t kLvglLockRetryDelaysMs[] = {100, 200, 400, 800, 1000};

static_assert(kUsPerSecond == 1000LL * kMsPerSecond,
              "UI microsecond and millisecond constants must stay consistent");
static_assert(kUiLoopBoundaryWakeSlackMs <= kMsPerSecond,
              "UI boundary wake slack must stay within one second");
static_assert(kNextSecondDelayMinMs > 0,
              "next-second delay minimum must be positive");
static_assert(kNextSecondDelayMaxMs >= kMsPerSecond,
              "next-second delay maximum must cover one second");
static_assert(kUsPerMinute == 60LL * kUsPerSecond,
              "UI minute conversion must stay consistent");
static_assert(kNextMinuteDelayMaxMs >= 60 * kMsPerSecond,
              "next-minute delay maximum must cover one minute");
static_assert(kLvglLockRetryDelaysMs[0] > 0,
              "LVGL lock retry delay must be positive");

void include_settings_deadline(uint32_t now,
                               uint32_t deadline,
                               bool enabled,
                               uint32_t *shortest)
{
    if (!enabled || !shortest) {
        return;
    }
    uint32_t remaining = app_tick_deadline_remaining(now, deadline);
    if (remaining == 0) {
        remaining = 1;
    }
    if (remaining < *shortest) {
        *shortest = remaining;
    }
}
} // namespace

uint32_t ui_next_second_delay_ms(int64_t sampled_wall_second,
                                 int64_t wall_clock_us)
{
    if (sampled_wall_second != wall_clock_us / kUsPerSecond) {
        return kNextSecondDelayMinMs;
    }
    int64_t second_offset_us = wall_clock_us % kUsPerSecond;
    int64_t until_next_us = kUsPerSecond - second_offset_us;
    uint32_t delay_ms = static_cast<uint32_t>(until_next_us / kMsPerSecond) +
                        kUiLoopBoundaryWakeSlackMs;
    if (delay_ms < kNextSecondDelayMinMs) {
        return kNextSecondDelayMinMs;
    }
    return delay_ms > kNextSecondDelayMaxMs ? kNextSecondDelayMaxMs : delay_ms;
}

bool ui_local_time_cache_refresh_due(int64_t sampled_wall_second,
                                     int64_t cached_wall_second,
                                     bool cache_valid)
{
    return !cache_valid || sampled_wall_second != cached_wall_second;
}

bool ui_low_battery_minute_idle(bool low_battery_mode,
                                bool auxiliary_page_requested)
{
    return low_battery_mode && !auxiliary_page_requested;
}

bool ui_low_refresh_page_minute_idle(bool page_uses_low_refresh,
                                     bool low_battery_mode,
                                     bool setup_portal_active,
                                     bool auxiliary_page_requested)
{
    return page_uses_low_refresh &&
           !low_battery_mode &&
           !setup_portal_active &&
           !auxiliary_page_requested;
}

bool ui_xiaozhi_activation_update_due(bool requested_active,
                                      bool previous_requested_active,
                                      bool previous_valid,
                                      bool allow_active_retry)
{
    return !previous_valid ||
           requested_active != previous_requested_active ||
           (requested_active && allow_active_retry);
}

uint32_t ui_next_minute_delay_ms(int64_t sampled_wall_second,
                                 int64_t wall_clock_us)
{
    if (sampled_wall_second != wall_clock_us / kUsPerSecond ||
        wall_clock_us < 0) {
        return kNextSecondDelayMinMs;
    }
    int64_t minute_offset_us = wall_clock_us % kUsPerMinute;
    int64_t until_next_us = kUsPerMinute - minute_offset_us;
    uint32_t delay_ms = static_cast<uint32_t>(until_next_us / kMsPerSecond) +
                        kUiLoopBoundaryWakeSlackMs;
    if (delay_ms < kNextSecondDelayMinMs) {
        return kNextSecondDelayMinMs;
    }
    return delay_ms > kNextMinuteDelayMaxMs ? kNextMinuteDelayMaxMs : delay_ms;
}

uint32_t ui_pomodoro_boundary_delay_ms(uint32_t boundary_ms)
{
    if (boundary_ms == 0) {
        return 0;
    }
    return boundary_ms + kUiLoopBoundaryWakeSlackMs;
}

uint32_t ui_lvgl_lock_retry_delay_ms(uint8_t consecutive_failures)
{
    if (consecutive_failures == 0) {
        return 0;
    }
    size_t index = static_cast<size_t>(consecutive_failures - 1U);
    constexpr size_t count =
        sizeof(kLvglLockRetryDelaysMs) / sizeof(kLvglLockRetryDelaysMs[0]);
    if (index >= count) {
        index = count - 1U;
    }
    return kLvglLockRetryDelaysMs[index];
}

uint32_t ui_shortest_delay_ticks(const uint32_t *candidates, size_t count)
{
    if (!candidates || count == 0) {
        return 0;
    }
    uint32_t shortest = candidates[0];
    for (size_t i = 1; i < count; ++i) {
        if (candidates[i] > 0 && (shortest == 0 || candidates[i] < shortest)) {
            shortest = candidates[i];
        }
    }
    return shortest;
}

uint32_t ui_inactivity_wait_ticks(uint32_t now_tick,
                                  uint32_t last_activity_tick,
                                  uint32_t timeout_ticks)
{
    return app_tick_nonzero_delay(app_tick_deadline_remaining(
        now_tick,
        last_activity_tick + timeout_ticks));
}

uint32_t ui_info_page_wait_ticks(const UiInfoPageWaitInput &input,
                                 uint32_t ota_active_fallback_ticks,
                                 uint32_t ota_updating_fallback_ticks)
{
    if (input.ota_updating) {
        return app_tick_nonzero_delay(ota_updating_fallback_ticks);
    }
    if (input.ota_flow_active) {
        return app_tick_nonzero_delay(ota_active_fallback_ticks);
    }
    if (input.hold_until_tick != 0) {
        return app_tick_nonzero_delay(app_tick_deadline_remaining(
            input.now_tick,
            input.hold_until_tick));
    }
    return UINT32_MAX;
}

uint32_t ui_settings_wait_ticks(const UiSettingsWaitInput &input,
                                uint32_t settings_timeout_ticks,
                                uint32_t ota_updating_fallback_ticks)
{
    uint32_t shortest = UINT32_MAX;
    include_settings_deadline(input.now_tick,
                              input.feedback_until_tick,
                              input.feedback_until_tick != 0,
                              &shortest);
    include_settings_deadline(input.now_tick,
                              input.sync_deadline_tick,
                              input.sync_busy && input.sync_deadline_tick != 0,
                              &shortest);
    include_settings_deadline(input.now_tick,
                              input.ota_status_until_tick,
                              input.ota_status_hold_set &&
                                  input.ota_status_until_tick != 0,
                              &shortest);
    include_settings_deadline(
        input.now_tick,
        input.last_activity_tick + settings_timeout_ticks,
        !input.sync_busy && !input.ota_flow_active && settings_timeout_ticks != 0,
        &shortest);
    if (input.ota_updating && ota_updating_fallback_ticks > 0 &&
        ota_updating_fallback_ticks < shortest) {
        shortest = ota_updating_fallback_ticks;
    }
    return shortest;
}
