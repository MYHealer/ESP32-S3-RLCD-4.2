// 刷新天气时钟运行期时间、日期、秒进度和整点提醒。
#include "ui_clock_runtime.h"

#include "audio_services.h"
#include "status_gif_contract.h"
#include "ui_clock_surface_objects.h"
#include "ui_clock_time.h"
#include "ui_dseg_clock.h"
#include "ui_draw_cache.h"
#include "ui_progress.h"
#include "ui_status_gif.h"
#include "ui_widgets.h"
#include "ui_work_status.h"
#include "work_page_ids.h"

namespace {

constexpr size_t kClockDateTextSize = 48;
int s_last_ui_second = -1;
int s_last_ui_minute = -1;
int s_last_ui_date_key = -1;
int s_last_ui_date_page = -1;
int s_last_second_progress_filled = -1;
int s_last_runtime_second_key = -1;
int s_last_runtime_date_key = -1;
int s_last_runtime_page = -1;

bool clock_runtime_update_due(const ClockUiTimeSnapshot &time_snapshot,
                              int active_work_page)
{
    return time_snapshot.second_key != s_last_runtime_second_key ||
           time_snapshot.date_key != s_last_runtime_date_key ||
           active_work_page != s_last_runtime_page;
}

void remember_clock_runtime_update(const ClockUiTimeSnapshot &time_snapshot,
                                   int active_work_page)
{
    s_last_runtime_second_key = time_snapshot.second_key;
    s_last_runtime_date_key = time_snapshot.date_key;
    s_last_runtime_page = active_work_page;
}

void invalidate_clock_runtime_update_cache()
{
    s_last_runtime_second_key = -1;
    s_last_runtime_date_key = -1;
    s_last_runtime_page = -1;
}

bool update_clock_minute_canvas(const ClockUiTimeSnapshot &time_snapshot,
                                const struct tm &local)
{
    if (time_snapshot.minute_key == s_last_ui_minute) {
        return false;
    }
    draw_time_canvas(local);
    s_last_ui_minute = time_snapshot.minute_key;
    return true;
}

bool update_clock_date_label(const ClockUiTimeSnapshot &time_snapshot,
                             const struct tm &local,
                             int date_page)
{
    if (time_snapshot.date_key == s_last_ui_date_key &&
        date_page == s_last_ui_date_page) {
        return false;
    }
    char date[kClockDateTextSize] = {};
    format_clock_date_text(date,
                           sizeof(date),
                           local,
                           time_snapshot.weekday);
    WorkPageStatusLabels labels = get_work_page_status_labels(date_page);
    const bool changed = set_label_text_if_changed(labels.date, date);
    s_last_ui_date_key = time_snapshot.date_key;
    s_last_ui_date_page = date_page;
    return changed;
}

} // namespace

void invalidate_clock_time_draw_cache()
{
    s_last_ui_second = -1;
    s_last_ui_minute = -1;
    invalidate_clock_date_draw_cache();
}

void invalidate_clock_date_draw_cache()
{
    s_last_ui_date_key = -1;
    s_last_ui_date_page = -1;
    invalidate_clock_runtime_update_cache();
}

void invalidate_clock_second_progress_draw_cache()
{
    s_last_second_progress_filled = -1;
}

bool update_time_ui(const struct tm &local,
                    const ClockUiTimeSnapshot &time_snapshot,
                    bool clock_page_active,
                    int active_work_page,
                    bool chime_enabled,
                    bool low_battery_mode,
                    bool setup_portal_active)
{
    bool changed = false;
    static int last_chime_hour_key = -1;
    if (!clock_runtime_update_due(time_snapshot, active_work_page)) {
        return false;
    }
    remember_clock_runtime_update(time_snapshot, active_work_page);
    if (clock_page_active) {
        changed |= update_clock_minute_canvas(time_snapshot, local);
    }
    if (clock_page_active &&
        !low_battery_mode &&
        local.tm_sec != s_last_ui_second) {
        draw_second_canvas(local);
        draw_status_gif_frame(local.tm_sec % STATUS_GIF_FRAME_COUNT);
        update_progress_canvas(clock_surface_object_refs().second_progress_canvas,
                               local.tm_sec + 1,
                               &s_last_second_progress_filled);
        s_last_ui_second = local.tm_sec;
        changed = true;
    }

    int date_page = (active_work_page == kWorkPageWeatherClock ||
                     low_battery_mode ||
                     setup_portal_active)
                        ? kWorkPageWeatherClock
                        : active_work_page;
    changed |= update_clock_date_label(time_snapshot, local, date_page);

    if (clock_hourly_chime_due(local,
                               time_snapshot,
                               chime_enabled,
                               low_battery_mode,
                               last_chime_hour_key)) {
        last_chime_hour_key = time_snapshot.hour_key;
        play_hourly_chime(local.tm_hour);
    }
    return changed;
}

bool update_setup_clock_header_time_ui(const struct tm &local)
{
    ClockUiTimeSnapshot time_snapshot = clock_ui_time_snapshot(local);
    bool changed = update_clock_minute_canvas(time_snapshot, local);
    changed |= update_clock_date_label(time_snapshot,
                                       local,
                                       kWorkPageWeatherClock);
    return changed;
}
