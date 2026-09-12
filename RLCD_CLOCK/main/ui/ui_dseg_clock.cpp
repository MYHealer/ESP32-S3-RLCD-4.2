// 格式化并刷新天气时钟的 DSEG 时分与秒 canvas。
#include "ui_dseg_clock.h"

#include "dseg_digits.h"
#include "ui_clock_surface_objects.h"
#include "ui_dseg_render.h"
#include "ui_text_format.h"

#include <stdio.h>

namespace {
constexpr int kDecimalBase = 10;
constexpr size_t kHourMinuteTextSize = sizeof("00:00");
constexpr size_t kSecondTextSize = sizeof("00");
constexpr const char *kHourMinuteFormat = "%02d:%02d";

void format_two_digit_second_text(char out[kSecondTextSize], int second)
{
    out[0] = static_cast<char>('0' + second / kDecimalBase);
    out[1] = static_cast<char>('0' + second % kDecimalBase);
    out[2] = '\0';
}

void format_hour_minute_text(char out[kHourMinuteTextSize], const struct tm &local)
{
    if (!out) {
        return;
    }
    int written = snprintf(out, kHourMinuteTextSize, kHourMinuteFormat, local.tm_hour, local.tm_min);
    if (ui_text::format_failed(written, kHourMinuteTextSize)) {
        out[0] = '\0';
    }
}
}

void draw_time_canvas(const struct tm &local)
{
    lv_obj_t *canvas = clock_surface_object_refs().time_canvas;
    if (!canvas) {
        return;
    }
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    char hm[kHourMinuteTextSize] = {};
    format_hour_minute_text(hm, local);
    draw_dseg_text(canvas, kDSEG84Font, hm, 0, 88);
    lv_obj_invalidate(canvas);
}

void draw_second_canvas(const struct tm &local)
{
    lv_obj_t *canvas = clock_surface_object_refs().second_canvas;
    if (!canvas) {
        return;
    }
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    char ss[kSecondTextSize] = {};
    format_two_digit_second_text(ss, local.tm_sec);
    draw_dseg_text(canvas, kDSEG36Font, ss, 0, 40);
    lv_obj_invalidate(canvas);
}
