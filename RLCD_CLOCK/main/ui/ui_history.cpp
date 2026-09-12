// 构建和刷新温湿历史页面及工作页顶部温湿度摘要。
#include "ui_work_pages.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "battery_runtime_state.h"
#include "sensor_services.h"
#include "sensor_time.h"
#include "work_page_ids.h"
#include "ui_battery.h"
#include "ui_canvas_primitives.h"
#include "ui_draw_cache.h"
#include "ui_fonts.h"
#include "ui_history_chart.h"
#include "ui_history_format.h"
#include "ui_history_layout.h"
#include "ui_history_window.h"
#include "ui_page_state.h"
#include "ui_progress.h"
#include "ui_widgets.h"
#include "ui_work_page_layout.h"
#include "ui_work_status.h"

#include <esp_attr.h>
#include <esp_log.h>

#include <string.h>

namespace {
using namespace ui_history_layout;

constexpr int kHoursPerDay = 24;

struct HistoryRedrawWorkspace {
    HourlySensorHistoryBlob history;
    HourlySensorSample samples[kHistoryWindowHours];
};

EXT_RAM_BSS_ATTR HistoryRedrawWorkspace s_history_redraw_workspace;

static_assert(sizeof(HistoryRedrawWorkspace) > 1024,
              "history redraw workspace should remain off the UI stack");

lv_color_t *s_history_chart_canvas_buffer;
lv_obj_t *s_history_chart_canvas;
lv_obj_t *s_history_temp_max_label;
lv_obj_t *s_history_temp_min_label;
lv_obj_t *s_history_humi_max_label;
lv_obj_t *s_history_humi_min_label;
EXT_RAM_BSS_ATTR lv_obj_t *s_history_time_labels[kAxisTickCount];
EXT_RAM_BSS_ATTR lv_obj_t *s_history_temp_axis_labels[kAxisValueCount];
EXT_RAM_BSS_ATTR lv_obj_t *s_history_humi_axis_labels[kAxisValueCount];
uint32_t s_last_history_drawn_version = static_cast<uint32_t>(-1);
int s_last_history_drawn_hour = -1;
static_assert(kHoursPerDay > 0, "Hours per day must be positive");
static_assert(array_count(s_history_time_labels) == kAxisTickCount,
              "History time label storage must match axis ticks");
static_assert(array_count(s_history_temp_axis_labels) == kAxisValueCount &&
                  array_count(s_history_humi_axis_labels) == kAxisValueCount,
              "History value label storage must match axis values");
#define HISTORY_TEMP_TITLE_CREATE_FAILED_LOG "history temp title create failed"
#define HISTORY_HUMI_TITLE_CREATE_FAILED_LOG "history humi title create failed"
#define HISTORY_CHART_CANVAS_CREATE_FAILED_LOG "history chart canvas create failed"
#define HISTORY_TIME_LABEL_CREATE_FAILED_FORMAT "history time label create failed index=%d"
#define HISTORY_TEMP_AXIS_LABEL_CREATE_FAILED_FORMAT "history temp axis label create failed index=%d"
#define HISTORY_HUMI_AXIS_LABEL_CREATE_FAILED_FORMAT "history humi axis label create failed index=%d"

} // namespace

void invalidate_history_draw_cache()
{
    s_last_history_drawn_version = static_cast<uint32_t>(-1);
    s_last_history_drawn_hour = -1;
}

enum class HistoryLabelLogKind {
    kTime,
    kTempAxis,
    kHumiAxis,
};

static lv_obj_t *make_history_title(lv_obj_t *screen,
                                    int y,
                                    const char *text,
                                    const char *failure_log)
{
    lv_obj_t *label = make_label(screen,
                                 kTitleX,
                                 y,
                                 kTitleWidth,
                                 kTitleHeight,
                                 text);
    if (label) {
        lv_obj_set_style_text_font(label, &zh_font_16, LV_PART_MAIN);
    } else {
        ESP_LOGW(TAG, "%s", failure_log);
    }
    return label;
}

static void build_history_value_badges(lv_obj_t *screen)
{
    lv_obj_t **badges[] = {
        &s_history_temp_max_label,
        &s_history_temp_min_label,
        &s_history_humi_max_label,
        &s_history_humi_min_label,
    };
    for (lv_obj_t **badge : badges) {
        *badge = make_label_with_font(screen,
                                      0,
                                      0,
                                      kBadgeWidth,
                                      kBadgeHeight,
                                      kAxisPlaceholder,
                                      &lv_font_montserrat_12);
    }
    for (lv_obj_t **badge : badges) {
        style_history_value_badge(*badge);
    }
}

int history_hour_key(const struct tm &local)
{
    return local.tm_yday * kHoursPerDay + local.tm_hour;
}

void align_history_label_or_log(lv_obj_t *label,
                                lv_text_align_t align,
                                HistoryLabelLogKind log_kind,
                                int index)
{
    if (label) {
        lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    } else {
        switch (log_kind) {
        case HistoryLabelLogKind::kTime:
            ESP_LOGW(TAG, HISTORY_TIME_LABEL_CREATE_FAILED_FORMAT, index);
            break;
        case HistoryLabelLogKind::kTempAxis:
            ESP_LOGW(TAG, HISTORY_TEMP_AXIS_LABEL_CREATE_FAILED_FORMAT, index);
            break;
        case HistoryLabelLogKind::kHumiAxis:
            ESP_LOGW(TAG, HISTORY_HUMI_AXIS_LABEL_CREATE_FAILED_FORMAT, index);
            break;
        }
    }
}

static __attribute__((noinline)) bool redraw_history_chart(time_t end_hour,
                                                           int hour_key)
{
    memset(&s_history_redraw_workspace, 0, sizeof(s_history_redraw_workspace));
    uint32_t history_version = 0;
    if (!get_hourly_sensor_history_snapshot(&s_history_redraw_workspace.history,
                                            &history_version)) {
        return false;
    }
    if (s_last_history_drawn_version == history_version &&
        s_last_history_drawn_hour == hour_key) {
        return false;
    }
    s_last_history_drawn_version = history_version;
    s_last_history_drawn_hour = hour_key;

    lv_canvas_fill_bg(s_history_chart_canvas, lv_color_white(), LV_OPA_COVER);

    int sample_count = 0;
    collect_history_window_from_snapshot(end_hour,
                                         s_history_redraw_workspace.history,
                                         s_history_redraw_workspace.samples,
                                         &sample_count);
    time_t start = end_hour - kHistoryWindowHours * kHistorySecondsPerHour;
    update_history_axis_labels(start, s_history_time_labels);

    draw_history_chart_panel(s_history_chart_canvas,
                             kCanvasWidth,
                             kCanvasHeight,
                             s_history_redraw_workspace.samples,
                             sample_count,
                             true,
                             kPlotX,
                             kTempPlotY,
                             kPlotWidth,
                             kPlotHeight,
                             s_history_temp_max_label,
                             s_history_temp_min_label,
                             s_history_temp_axis_labels);
    draw_history_chart_panel(s_history_chart_canvas,
                             kCanvasWidth,
                             kCanvasHeight,
                             s_history_redraw_workspace.samples,
                             sample_count,
                             false,
                             kPlotX,
                             kHumiPlotY,
                             kPlotWidth,
                             kPlotHeight,
                             s_history_humi_max_label,
                             s_history_humi_min_label,
                             s_history_humi_axis_labels);
    lv_obj_invalidate(s_history_chart_canvas);
    return true;
}

bool update_history_page(const struct tm &local)
{
    build_history_page();
    if (!s_history_chart_canvas) {
        return false;
    }
    bool changed = update_work_page_status_time(kWorkPageHistory, local);
    const int hour_key = history_hour_key(local);
    if (s_last_history_drawn_version == get_hourly_sensor_history_version() &&
        s_last_history_drawn_hour == hour_key) {
        return changed;
    }
    struct tm mutable_local = local;
    const time_t now = mktime(&mutable_local);
    const time_t end_hour = hour_start_from_time(now);
    changed |= redraw_history_chart(end_hour, hour_key);
    return changed;
}

static bool ensure_history_chart_canvas(lv_obj_t *screen)
{
    if (!screen) {
        return false;
    }
    if (!s_history_chart_canvas_buffer) {
        s_history_chart_canvas_buffer =
            alloc_canvas_buffer(kCanvasWidth, kCanvasHeight);
    }
    if (!s_history_chart_canvas_buffer) {
        return false;
    }
    if (!s_history_chart_canvas) {
        s_history_chart_canvas = lv_canvas_create(screen);
        if (!s_history_chart_canvas) {
            ESP_LOGW(TAG, "%s", HISTORY_CHART_CANVAS_CREATE_FAILED_LOG);
            return false;
        }
        configure_canvas_base(s_history_chart_canvas,
                              s_history_chart_canvas_buffer,
                              kCanvasX,
                              kCanvasY,
                              kCanvasWidth,
                              kCanvasHeight);
        lv_canvas_fill_bg(s_history_chart_canvas,
                          lv_color_white(),
                          LV_OPA_COVER);
        invalidate_history_draw_cache();
    }
    return true;
}

static void build_history_chart_area(lv_obj_t *screen)
{
    if (!screen) {
        return;
    }
    make_black_bar(screen,
                   ui_work_page_layout::kTopSeparatorX,
                   ui_work_page_layout::kTopSeparatorY,
                   ui_work_page_layout::kTopSeparatorWidth,
                   ui_work_page_layout::kTopSeparatorHeight);
    lv_obj_t *temp_title = make_history_title(screen,
                                              kTempTitleY,
                                              kTempTitle,
                                              HISTORY_TEMP_TITLE_CREATE_FAILED_LOG);
    lv_obj_t *humi_title = make_history_title(screen,
                                              kHumiTitleY,
                                              kHumiTitle,
                                              HISTORY_HUMI_TITLE_CREATE_FAILED_LOG);

    ensure_history_chart_canvas(screen);
    if (temp_title) {
        lv_obj_move_foreground(temp_title);
    }
    if (humi_title) {
        lv_obj_move_foreground(humi_title);
    }
}

static void build_history_axis_labels(lv_obj_t *screen)
{
    if (!screen) {
        return;
    }
    for (int i = 0; i < kAxisTickCount; ++i) {
        s_history_time_labels[i] = make_label_with_font(screen,
                                                        kTimeLabelCenterX[i] - kTimeLabelWidth / 2,
                                                        kTimeLabelY,
                                                        kTimeLabelWidth,
                                                        kTimeLabelHeight,
                                                        kTimePlaceholder,
                                                        &lv_font_montserrat_14);
        align_history_label_or_log(s_history_time_labels[i],
                                   LV_TEXT_ALIGN_CENTER,
                                   HistoryLabelLogKind::kTime,
                                   i);
    }
    for (int i = 0; i < kAxisValueCount; ++i) {
        s_history_temp_axis_labels[i] = make_label(screen,
                                                   kAxisLabelX,
                                                   kTempAxisLabelY + i * kAxisLabelRowGap,
                                                   kAxisLabelWidth,
                                                   kAxisLabelHeight,
                                                   kAxisPlaceholder);
        s_history_humi_axis_labels[i] = make_label(screen,
                                                   kAxisLabelX,
                                                   kHumiAxisLabelY + i * kAxisLabelRowGap,
                                                   kAxisLabelWidth,
                                                   kAxisLabelHeight,
                                                   kAxisPlaceholder);
        align_history_label_or_log(s_history_temp_axis_labels[i],
                                   LV_TEXT_ALIGN_LEFT,
                                   HistoryLabelLogKind::kTempAxis,
                                   i);
        align_history_label_or_log(s_history_humi_axis_labels[i],
                                   LV_TEXT_ALIGN_LEFT,
                                   HistoryLabelLogKind::kHumiAxis,
                                   i);
    }
}

void build_history_page()
{
    lv_obj_t *existing_root = work_page_root(kWorkPageHistory);
    if (existing_root) {
        ensure_history_chart_canvas(existing_root);
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_work_page_root(kWorkPageHistory, screen);

    build_work_page_battery_icon(screen, kWorkPageHistory);
    build_work_page_status_bar(screen,
                               kWorkPageHistory,
                               true,
                               true);

    build_history_chart_area(screen);
    build_work_page_day_progress(screen, kWorkPageHistory);
    build_history_axis_labels(screen);
    build_history_value_badges(screen);

    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    update_work_page_battery_icon(kWorkPageHistory, battery_percent_load());
}

void clear_history_object_refs()
{
    s_history_chart_canvas = nullptr;
    s_history_temp_max_label = nullptr;
    s_history_temp_min_label = nullptr;
    s_history_humi_max_label = nullptr;
    s_history_humi_min_label = nullptr;
    for (lv_obj_t *&label : s_history_time_labels) {
        label = nullptr;
    }
    for (lv_obj_t *&label : s_history_temp_axis_labels) {
        label = nullptr;
    }
    for (lv_obj_t *&label : s_history_humi_axis_labels) {
        label = nullptr;
    }
    invalidate_history_draw_cache();
}
