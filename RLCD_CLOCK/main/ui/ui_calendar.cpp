// 构建和刷新当月日历页面及其农历节日显示。
#include "ui_work_pages.h"

#include "app_metadata.h"
#include "app_time_constants.h"
#include "battery_runtime_state.h"
#include "calendar_lunar.h"
#include "work_page_ids.h"
#include "ui_battery.h"
#include "ui_calendar_layout.h"
#include "ui_canvas_primitives.h"
#include "ui_fonts.h"
#include "ui_page_state.h"
#include "ui_progress.h"
#include "ui_widgets.h"
#include "ui_work_page_layout.h"
#include "ui_work_status.h"

#include <esp_log.h>

#define CALENDAR_CANVAS_CREATE_FAILED_LOG "calendar canvas create failed"
#define CALENDAR_LAYOUT_INVALID_LOG "calendar layout invalid"

using namespace ui_calendar_view_layout;

static lv_obj_t *s_calendar_canvas;
static lv_color_t *s_calendar_canvas_buffer;
static int s_last_calendar_drawn_month = -1;
static int s_last_calendar_drawn_day = -1;

static void canvas_fill_rect_safe(lv_img_dsc_t *image,
                                  int w,
                                  int h,
                                  int x,
                                  int y,
                                  int rw,
                                  int rh,
                                  lv_color_t color)
{
    for (int yy = y; yy < y + rh; ++yy) {
        for (int xx = x; xx < x + rw; ++xx) {
            canvas_image_set_px_safe(image, xx, yy, w, h, color);
        }
    }
}

static void canvas_dot_rect(lv_img_dsc_t *image,
                            int w,
                            int h,
                            int x,
                            int y,
                            int rw,
                            int rh)
{
    for (int yy = y; yy < y + rh; yy += kDottedFillYStep) {
        for (int xx = x; xx < x + rw; xx += kDottedFillXStep) {
            canvas_image_set_px_safe(image, xx, yy, w, h, lv_color_black());
        }
    }
    int right = x + rw - 1;
    int bottom = y + rh - 1;
    for (int yy = y; yy <= bottom; yy += kDottedFillYStep) {
        canvas_image_set_px_safe(image, right, yy, w, h, lv_color_black());
    }
    for (int xx = x; xx <= right; xx += kDottedFillXStep) {
        canvas_image_set_px_safe(image, xx, bottom, w, h, lv_color_black());
    }
    canvas_image_set_px_safe(image, right, bottom, w, h, lv_color_black());
}

static void canvas_fill_round_rect_safe(lv_img_dsc_t *image,
                                        int w,
                                        int h,
                                        int x,
                                        int y,
                                        int rw,
                                        int rh,
                                        int radius,
                                        lv_color_t color)
{
    int r2 = radius * radius;
    for (int yy = 0; yy < rh; ++yy) {
        for (int xx = 0; xx < rw; ++xx) {
            int dx = 0;
            if (xx < radius) {
                dx = radius - xx;
            } else if (xx >= rw - radius) {
                dx = xx - (rw - radius - 1);
            }
            int dy = 0;
            if (yy < radius) {
                dy = radius - yy;
            } else if (yy >= rh - radius) {
                dy = yy - (rh - radius - 1);
            }
            if (dx == 0 || dy == 0 || dx * dx + dy * dy <= r2) {
                canvas_image_set_px_safe(image, x + xx, y + yy, w, h, color);
            }
        }
    }
}

static void draw_calendar_text(lv_obj_t *canvas,
                               const char *text,
                               int x,
                               int y,
                               int w,
                               int h,
                               const lv_font_t *font,
                               lv_color_t color,
                               lv_text_align_t align)
{
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = font;
    dsc.align = align;
    lv_canvas_draw_text(canvas, x, y, w, &dsc, text);
}

static void draw_calendar_weekday_header(lv_img_dsc_t *image)
{
    canvas_fill_rect_safe(image,
                          kCanvasWidth,
                          kCanvasHeight,
                          kGridX,
                          kHeaderY,
                          kCellWidth,
                          kHeaderHeight,
                          lv_color_black());
    canvas_fill_rect_safe(image,
                          kCanvasWidth,
                          kCanvasHeight,
                          kGridX + kCellWidth * kSaturdayColumn,
                          kHeaderY,
                          kCellWidth,
                          kHeaderHeight,
                          lv_color_black());
    canvas_dot_rect(image,
                    kCanvasWidth,
                    kCanvasHeight,
                    kGridX + kCellWidth,
                    kHeaderY,
                    kCellWidth * (kCalendarWeekdayCount - 2),
                    kHeaderHeight);

    for (int col = 0; col < kCalendarWeekdayCount; ++col) {
        int x = kGridX + col * kCellWidth;
        if (col == kSundayColumn || col == kSaturdayColumn) {
            draw_calendar_text(s_calendar_canvas,
                               kWeekdays[col],
                               x,
                               kHeaderY,
                               kCellWidth,
                               kHeaderHeight,
                               &zh_font_16,
                               lv_color_white(),
                               LV_TEXT_ALIGN_CENTER);
        } else {
            draw_calendar_text(s_calendar_canvas,
                               kWeekdays[col],
                               x,
                               kHeaderY,
                               kCellWidth,
                               kHeaderHeight,
                               &zh_font_16,
                               lv_color_black(),
                               LV_TEXT_ALIGN_CENTER);
        }
    }
}

static void draw_calendar_day_cell(const struct tm &local,
                                   const CalendarMonthLayout &layout,
                                   lv_img_dsc_t *image,
                                   int day)
{
    int display_row = -1;
    int col = -1;
    if (!calendar_day_display_position(layout, day, &display_row, &col)) {
        return;
    }
    int x = kGridX + col * kCellWidth;
    int y = kCellY + display_row * kCellHeight;
    bool is_today = day == local.tm_mday;

    if (is_today) {
        canvas_fill_round_rect_safe(image,
                                    kCanvasWidth,
                                    kCanvasHeight,
                                    x + kTodayInsetX,
                                    y + kTodayInsetTop,
                                    kCellWidth - kTodayInsetWidth,
                                    kCellHeight - kTodayInsetHeight,
                                    kTodayRadius,
                                    lv_color_black());
    }

    char day_text[kDayTextSize] = {};
    format_calendar_day_text(day_text, sizeof(day_text), day);
    draw_calendar_text(s_calendar_canvas,
                       day_text,
                       x + kDayTextXInset,
                       y + kDayTextY,
                       kCellWidth - kDayTextWidthInset,
                       kDayTextHeight,
                       &lv_font_montserrat_16,
                       is_today ? lv_color_white() : lv_color_black(),
                       LV_TEXT_ALIGN_CENTER);

    struct tm day_tm = local;
    day_tm.tm_mday = day;
    day_tm.tm_hour = 12;
    day_tm.tm_min = 0;
    day_tm.tm_sec = 0;
    mktime(&day_tm);
    CalendarDayInfo info;
    calendar_day_info(day_tm, &info);
    draw_calendar_text(s_calendar_canvas,
                       info.subtext,
                       x + kDayTextXInset,
                       y + kSubTextY,
                       kCellWidth - kDayTextWidthInset,
                       kSubTextHeight,
                       &zh_font_16,
                       is_today ? lv_color_white() : lv_color_black(),
                       LV_TEXT_ALIGN_CENTER);
}

static void draw_calendar_grid(const struct tm &local)
{
    if (!s_calendar_canvas) {
        return;
    }
    lv_canvas_fill_bg(s_calendar_canvas, lv_color_white(), LV_OPA_COVER);
    lv_img_dsc_t *image = lv_canvas_get_img(s_calendar_canvas);
    draw_calendar_weekday_header(image);

    int year = local.tm_year + kTmYearOffset;
    int month = local.tm_mon + kTmMonthOffset;
    int today = local.tm_mday;
    int first_weekday = calendar_first_weekday(year, month);
    int days = calendar_days_in_month(year, month);
    CalendarMonthLayout layout = {};
    if (!calculate_calendar_month_layout(first_weekday, days, today, &layout)) {
        ESP_LOGW(TAG, "%s", CALENDAR_LAYOUT_INVALID_LOG);
        lv_obj_invalidate(s_calendar_canvas);
        return;
    }
    for (int day = 1; day <= days; ++day) {
        draw_calendar_day_cell(local, layout, image, day);
    }
    lv_obj_invalidate(s_calendar_canvas);
}

bool update_calendar_page(const struct tm &local)
{
    build_calendar_page();
    bool changed = false;
    int month_key = calendar_month_key(local);
    if (month_key != s_last_calendar_drawn_month || local.tm_mday != s_last_calendar_drawn_day) {
        s_last_calendar_drawn_month = month_key;
        s_last_calendar_drawn_day = local.tm_mday;
        draw_calendar_grid(local);
        changed = true;
    }
    changed |= update_work_page_status_time(kWorkPageCalendar, local);
    return changed;
}

static bool ensure_calendar_canvas(lv_obj_t *screen)
{
    if (!screen) {
        return false;
    }
    if (!s_calendar_canvas_buffer) {
        s_calendar_canvas_buffer =
            alloc_canvas_buffer(kCanvasWidth, kCanvasHeight);
    }
    if (!s_calendar_canvas_buffer) {
        return false;
    }
    if (!s_calendar_canvas) {
        s_calendar_canvas = lv_canvas_create(screen);
        if (!s_calendar_canvas) {
            ESP_LOGW(TAG, "%s", CALENDAR_CANVAS_CREATE_FAILED_LOG);
            return false;
        }
        configure_canvas_base(s_calendar_canvas,
                              s_calendar_canvas_buffer,
                              kCanvasX,
                              kCanvasY,
                              kCanvasWidth,
                              kCanvasHeight);
        lv_canvas_fill_bg(s_calendar_canvas,
                          lv_color_white(),
                          LV_OPA_COVER);
        s_last_calendar_drawn_month = -1;
        s_last_calendar_drawn_day = -1;
    }
    return true;
}

void build_calendar_page()
{
    lv_obj_t *existing_root = work_page_root(kWorkPageCalendar);
    if (existing_root) {
        ensure_calendar_canvas(existing_root);
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_work_page_root(kWorkPageCalendar, screen);

    build_work_page_battery_icon(screen, kWorkPageCalendar);
    build_work_page_status_bar(screen,
                               kWorkPageCalendar,
                               true,
                               true);

    make_black_bar(screen,
                   ui_work_page_layout::kTopSeparatorX,
                   ui_work_page_layout::kTopSeparatorY,
                   ui_work_page_layout::kTopSeparatorWidth,
                   ui_work_page_layout::kTopSeparatorHeight);
    build_work_page_day_progress(screen, kWorkPageCalendar);

    ensure_calendar_canvas(screen);

    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    update_work_page_battery_icon(kWorkPageCalendar, battery_percent_load());
}

void clear_calendar_object_refs()
{
    s_calendar_canvas = nullptr;
    s_last_calendar_drawn_month = -1;
    s_last_calendar_drawn_day = -1;
}
