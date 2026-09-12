// 组合刷新当前工作页的时间、正文、全天进度和天气预警。
#include "ui_work_page_update.h"
#include "ui_aggregate_clock.h"

#include "sensor_time.h"
#include "ui_clock_alert_state.h"
#include "ui_clock_runtime.h"
#include "ui_clock_time.h"
#include "ui_draw_cache.h"
#include "ui_flip_clock.h"
#include "ui_page_state.h"
#include "ui_progress.h"
#include "ui_widgets.h"
#include "ui_work_pages.h"
#include "ui_work_status.h"
#include "ui_xiaozhi.h"
#include "weather_state.h"
#include "work_page_ids.h"

namespace {
constexpr const char *kUiDatePlaceholder = "----/--/-- / 星期-";
constexpr const char *kUiTimePlaceholder = "--:--";
bool update_visible_work_page_body(const struct tm &local,
                                   const ClockUiTimeSnapshot &time_snapshot,
                                   const ActiveWorkPageState &state)
{
    bool changed = false;
    if (state.aggregate_clock) {
        changed |= update_aggregate_clock_page(local);
    }
    if (state.history) {
        changed |= update_history_page(local);
    }
    if (state.gallery) {
        changed |= update_gallery_page(local);
    }
    if (state.calendar) {
        changed |= update_calendar_page(local);
    }
    if (state.weather_board) {
        changed |= update_weather_board_page(local);
    }
    if (state.flip_clock) {
        changed |= update_flip_clock_page(local, time_snapshot);
    }
    if (state.xiaozhi) {
        changed |= update_xiaozhi_page(local);
    }
    return changed;
}

bool update_weather_alert_state(const struct tm &local,
                                const ActiveWorkPageState &state,
                                bool status_due,
                                const UiStatusRefreshSnapshot &status,
                                bool low_battery_mode,
                                bool setup_active,
                                bool &alert_visible,
                                int &alert_index,
                                uint32_t &alert_version)
{
    if (!state.weather_clock) {
        if (!alert_visible) {
            return false;
        }
        update_alert_pill(false,
                          0,
                          status,
                          low_battery_mode,
                          setup_active);
        alert_visible = false;
        alert_index = -1;
        return true;
    }

    const WeatherAlertStatusSnapshot alert =
        weather_alert_status_snapshot_load();
    const bool alert_changed = alert.version != alert_version;
    alert_version = alert.version;
    ClockAlertDisplayState next_alert = clock_alert_display_state(local.tm_sec,
                                                                 low_battery_mode,
                                                                 alert.active,
                                                                 alert.count);
    if (!clock_alert_display_needs_update(next_alert,
                                          alert_visible,
                                          alert_index,
                                          status_due || alert_changed)) {
        return false;
    }
    update_alert_pill(next_alert.visible,
                      next_alert.index,
                      status,
                      low_battery_mode,
                      setup_active);
    alert_visible = next_alert.visible;
    alert_index = next_alert.visible ? next_alert.index : -1;
    return true;
}
} // namespace

bool update_active_work_page_invalid_time_labels(int active_work_page,
                                                 bool force_weather_clock_status)
{
    int status_page = force_weather_clock_status
                          ? kWorkPageWeatherClock
                          : active_work_page;
    WorkPageStatusLabels labels = get_work_page_status_labels(status_page);
    invalidate_work_page_status_time_cache(status_page);
    bool changed = set_label_text_if_changed(labels.date, kUiDatePlaceholder);
    changed |= set_label_text_if_changed(labels.time, kUiTimePlaceholder);
    return changed;
}

bool update_active_work_page_content(const struct tm &local,
                                     const ActiveWorkPageState &state,
                                     int active_page,
                                     bool status_due,
                                     const UiStatusRefreshSnapshot &status,
                                     bool low_battery_mode,
                                     bool setup_portal_active,
                                     bool &alert_visible,
                                     int &alert_index,
                                     uint32_t &alert_version)
{
    bool changed = false;
    if (is_tm_plausible(local)) {
        const ClockUiTimeSnapshot time_snapshot = clock_ui_time_snapshot(local);
        changed |= update_time_ui(local,
                                  time_snapshot,
                                  state.weather_clock,
                                  active_page,
                                  status.chime_enabled,
                                  low_battery_mode,
                                  setup_portal_active);
        changed |= update_visible_work_page_body(local, time_snapshot, state);
        changed |= update_work_page_day_progress(active_page, time_snapshot);
        changed |= update_weather_alert_state(local,
                                              state,
                                              status_due,
                                              status,
                                              low_battery_mode,
                                              setup_portal_active,
                                              alert_visible,
                                              alert_index,
                                              alert_version);
        return changed;
    }

    changed |= update_active_work_page_invalid_time_labels(
        active_page,
        low_battery_mode || setup_portal_active);
    if (state.aggregate_clock) changed |= update_aggregate_clock_page(local);
    invalidate_clock_date_draw_cache();
    update_alert_pill(false,
                      0,
                      status,
                      low_battery_mode,
                      setup_portal_active);
    if (alert_visible) {
        alert_visible = false;
        alert_index = -1;
        changed = true;
    }
    return changed;
}
