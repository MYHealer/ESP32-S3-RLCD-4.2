// 实现 SDL 温湿历史页面主体，保持设备预览的曲线和标记布局集中维护。
#include "sdl_preview_history.h"

#include <stdio.h>

#include <vector>

#include "sdl_preview_widgets.h"
#include "ui_history_layout.h"

LV_FONT_DECLARE(zh_font_16);

namespace {
using sdl_preview_widgets::canvas_draw_filled_circle;
using sdl_preview_widgets::canvas_draw_line;
using sdl_preview_widgets::canvas_set_px_safe;
using sdl_preview_widgets::make_label;
using sdl_preview_widgets::make_label_with_font;
using sdl_preview_widgets::set_label_text_if_changed;
using namespace ui_history_layout;

std::vector<lv_color_t> g_history_chart_canvas_pixels(kCanvasWidth * kCanvasHeight);

struct PreviewHistorySample {
    float temp;
    float humi;
};

int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

void canvas_draw_dashed_hline(lv_obj_t *canvas,
                              int w,
                              int h,
                              int x1,
                              int x2,
                              int y,
                              lv_color_t color)
{
    for (int x = x1; x <= x2; ++x) {
        if (((x - x1) / 5) % 2 == 0) {
            canvas_set_px_safe(canvas, x, y, w, h, color);
        }
    }
}

int value_to_plot_y(float value, float min_value, float max_value, int y, int h)
{
    float range = max_value - min_value;
    if (range < 0.01f) {
        range = 1.0f;
    }
    float normalized = (value - min_value) / range;
    int offset = static_cast<int>(normalized * (h - 1) + 0.5f);
    return y + h - 1 - offset;
}

void style_history_badge(lv_obj_t *label)
{
    lv_obj_set_style_bg_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(label, kBadgeRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_left(label, kBadgeHorizontalPad, LV_PART_MAIN);
    lv_obj_set_style_pad_right(label, kBadgeHorizontalPad, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 0, LV_PART_MAIN);
}

void place_badge(lv_obj_t *label,
                 const char *text,
                 int point_x,
                 int point_y,
                 int plot_x,
                 int plot_y,
                 int plot_w,
                 int plot_h)
{
    set_label_text_if_changed(label, text);
    int x = kCanvasX + point_x - kBadgeWidth / 2;
    int min_y = kCanvasY + plot_y;
    int y = kCanvasY + point_y - kBadgeHeight - kBadgePointGap;
    if (y < min_y) {
        y = kCanvasY + point_y + kBadgePointGap;
    }
    x = clamp_int(x,
                  kCanvasX + plot_x,
                  kCanvasX + plot_x + plot_w - kBadgeWidth);
    y = clamp_int(y,
                  min_y,
                  kCanvasY + plot_y + plot_h - kBadgeHeight);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, kBadgeWidth, kBadgeHeight);
}

void format_axis_hour(time_t value, char *out, size_t out_len)
{
    struct tm local = {};
    localtime_r(&value, &local);
    snprintf(out, out_len, "%02d:00", local.tm_hour);
}

void draw_preview_history_panel(lv_obj_t *canvas,
                                const PreviewHistorySample *samples,
                                bool temperature,
                                int plot_x,
                                int plot_y,
                                int plot_w,
                                int plot_h,
                                lv_obj_t *max_label,
                                lv_obj_t *min_label,
                                lv_obj_t **axis_labels)
{
    float min_value = temperature ? samples[0].temp : samples[0].humi;
    float max_value = min_value;
    int min_index = 0;
    int max_index = 0;
    for (int i = 1; i < kDisplayedWindowHours; ++i) {
        float value = temperature ? samples[i].temp : samples[i].humi;
        if (value < min_value) {
            min_value = value;
            min_index = i;
        }
        if (value > max_value) {
            max_value = value;
            max_index = i;
        }
    }
    for (int i = 0; i < kGridLineCount; ++i) {
        int y = plot_y + (plot_h * i) / kGridIntervalCount;
        canvas_draw_dashed_hline(canvas,
                                 kCanvasWidth,
                                 kCanvasHeight,
                                 plot_x,
                                 plot_x + plot_w,
                                 y,
                                 lv_color_black());
    }
    canvas_draw_line(canvas,
                     kCanvasWidth,
                     kCanvasHeight,
                     plot_x,
                     plot_y + plot_h,
                     plot_x + plot_w,
                     plot_y + plot_h,
                     lv_color_black());

    float pad = temperature ? 0.6f : 3.0f;
    float axis_min = min_value - pad;
    float axis_max = max_value + pad;
    float axis_mid = (axis_min + axis_max) * 0.5f;
    char text[16];
    snprintf(text, sizeof(text), temperature ? "%.0f℃" : "%.0f%%", axis_max);
    set_label_text_if_changed(axis_labels[0], text);
    snprintf(text, sizeof(text), temperature ? "%.0f℃" : "%.0f%%", axis_mid);
    set_label_text_if_changed(axis_labels[1], text);
    snprintf(text, sizeof(text), temperature ? "%.0f℃" : "%.0f%%", axis_min);
    set_label_text_if_changed(axis_labels[2], text);

    int prev_x = 0;
    int prev_y = 0;
    for (int i = 0; i < kDisplayedWindowHours; ++i) {
        int x = plot_x + ((i + 1) * plot_w) / kDisplayedWindowHours;
        float value = temperature ? samples[i].temp : samples[i].humi;
        int y = value_to_plot_y(value, axis_min, axis_max, plot_y, plot_h);
        if (i > 0) {
            canvas_draw_line(canvas,
                             kCanvasWidth,
                             kCanvasHeight,
                             prev_x,
                             prev_y,
                             x,
                             y,
                             lv_color_black());
        }
        prev_x = x;
        prev_y = y;
    }

    snprintf(text,
             sizeof(text),
             temperature ? "%.1f" : "%.0f",
             temperature ? samples[max_index].temp : samples[max_index].humi);
    int max_x = plot_x + ((max_index + 1) * plot_w) / kDisplayedWindowHours;
    int max_y = value_to_plot_y(max_value, axis_min, axis_max, plot_y, plot_h);
    canvas_draw_filled_circle(canvas,
                              kCanvasWidth,
                              kCanvasHeight,
                              max_x,
                              max_y,
                              kPointRadius,
                              lv_color_black());
    place_badge(max_label, text, max_x, max_y, plot_x, plot_y, plot_w, plot_h);

    snprintf(text,
             sizeof(text),
             temperature ? "%.1f" : "%.0f",
             temperature ? samples[min_index].temp : samples[min_index].humi);
    int min_x = plot_x + ((min_index + 1) * plot_w) / kDisplayedWindowHours;
    int min_y = value_to_plot_y(min_value, axis_min, axis_max, plot_y, plot_h);
    canvas_draw_filled_circle(canvas,
                              kCanvasWidth,
                              kCanvasHeight,
                              min_x,
                              min_y,
                              kPointRadius,
                              lv_color_black());
    place_badge(min_label, text, min_x, min_y, plot_x, plot_y, plot_w, plot_h);
}
} // namespace

void build_history_preview_body(lv_obj_t *screen, struct tm *local)
{
    if (!screen || !local) {
        return;
    }

    lv_obj_t *temp_title =
        make_label(screen, kTitleX, kTempTitleY, kTitleWidth, kTitleHeight, kTempTitle);
    lv_obj_set_style_text_font(temp_title, &zh_font_16, LV_PART_MAIN);
    lv_obj_t *humi_title =
        make_label(screen, kTitleX, kHumiTitleY, kTitleWidth, kTitleHeight, kHumiTitle);
    lv_obj_set_style_text_font(humi_title, &zh_font_16, LV_PART_MAIN);

    lv_obj_t *chart = lv_canvas_create(screen);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(chart, kCanvasX, kCanvasY);
    lv_obj_set_size(chart, kCanvasWidth, kCanvasHeight);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(chart,
                         g_history_chart_canvas_pixels.data(),
                         kCanvasWidth,
                         kCanvasHeight,
                         LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(chart, lv_color_white(), LV_OPA_COVER);
    lv_obj_move_foreground(temp_title);
    lv_obj_move_foreground(humi_title);

    local->tm_min = 0;
    local->tm_sec = 0;
    time_t end_hour = mktime(local);
    time_t start_hour = end_hour - kDisplayedWindowHours * kDisplayedSecondsPerHour;
    for (int i = 0; i < kAxisTickCount; ++i) {
        char text[8];
        format_axis_hour(start_hour + kAxisTickHours[i] * kDisplayedSecondsPerHour,
                         text,
                         sizeof(text));
        lv_obj_t *label = make_label_with_font(screen,
                                               kTimeLabelCenterX[i] - kTimeLabelWidth / 2,
                                               kTimeLabelY,
                                               kTimeLabelWidth,
                                               kTimeLabelHeight,
                                               text,
                                               &lv_font_montserrat_14);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    lv_obj_t *temp_axis[kAxisValueCount] = {};
    lv_obj_t *humi_axis[kAxisValueCount] = {};
    for (int i = 0; i < kAxisValueCount; ++i) {
        temp_axis[i] = make_label(screen,
                                  kAxisLabelX,
                                  kTempAxisLabelY + i * kAxisLabelRowGap,
                                  kAxisLabelWidth,
                                  kAxisLabelHeight,
                                  kAxisPlaceholder);
        humi_axis[i] = make_label(screen,
                                  kAxisLabelX,
                                  kHumiAxisLabelY + i * kAxisLabelRowGap,
                                  kAxisLabelWidth,
                                  kAxisLabelHeight,
                                  kAxisPlaceholder);
        lv_obj_set_style_text_align(temp_axis[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_set_style_text_align(humi_axis[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    }

    lv_obj_t *temp_max = make_label_with_font(
        screen, 0, 0, kBadgeWidth, kBadgeHeight, kAxisPlaceholder, &lv_font_montserrat_12);
    lv_obj_t *temp_min = make_label_with_font(
        screen, 0, 0, kBadgeWidth, kBadgeHeight, kAxisPlaceholder, &lv_font_montserrat_12);
    lv_obj_t *humi_max = make_label_with_font(
        screen, 0, 0, kBadgeWidth, kBadgeHeight, kAxisPlaceholder, &lv_font_montserrat_12);
    lv_obj_t *humi_min = make_label_with_font(
        screen, 0, 0, kBadgeWidth, kBadgeHeight, kAxisPlaceholder, &lv_font_montserrat_12);
    style_history_badge(temp_max);
    style_history_badge(temp_min);
    style_history_badge(humi_max);
    style_history_badge(humi_min);

    constexpr PreviewHistorySample kSamples[kDisplayedWindowHours] = {
        {26.9f, 58}, {26.8f, 57}, {27.2f, 64}, {27.7f, 64}, {26.8f, 59}, {26.3f, 64},
        {26.2f, 64}, {26.3f, 65}, {26.3f, 65}, {26.4f, 66}, {26.4f, 66}, {26.4f, 65},
        {26.2f, 66}, {26.3f, 65}, {26.0f, 66}, {26.1f, 65}, {26.0f, 66}, {25.9f, 68},
        {26.1f, 66}, {26.2f, 65}, {26.4f, 65}, {26.6f, 63}, {26.8f, 63}, {27.0f, 62},
    };
    draw_preview_history_panel(chart,
                               kSamples,
                               true,
                               kPlotX,
                               kTempPlotY,
                               kPlotWidth,
                               kPlotHeight,
                               temp_max,
                               temp_min,
                               temp_axis);
    draw_preview_history_panel(chart,
                               kSamples,
                               false,
                               kPlotX,
                               kHumiPlotY,
                               kPlotWidth,
                               kPlotHeight,
                               humi_max,
                               humi_min,
                               humi_axis);
    lv_obj_invalidate(chart);
}
