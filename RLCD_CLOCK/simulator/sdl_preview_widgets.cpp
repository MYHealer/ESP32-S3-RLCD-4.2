// 实现 SDL 预览页面共用的基础 LVGL 标签和黑白条控件。
#include "sdl_preview_widgets.h"

#include <string.h>

LV_FONT_DECLARE(zh_font_16);

namespace sdl_preview_widgets {

void set_obj_black(lv_obj_t *obj, bool active)
{
    lv_obj_set_style_bg_color(obj, active ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 1, LV_PART_MAIN);
}

lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    set_obj_black(bar, false);
    return bar;
}

lv_obj_t *make_black_bar(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *bar = make_bar(parent, x, y, w, h);
    set_obj_black(bar, true);
    return bar;
}

lv_obj_t *make_canvas(lv_obj_t *parent,
                      int x,
                      int y,
                      int width,
                      int height,
                      lv_color_t *pixels)
{
    if (!parent || !pixels || width <= 0 || height <= 0) {
        return nullptr;
    }
    lv_obj_t *canvas = lv_canvas_create(parent);
    if (!canvas) {
        return nullptr;
    }
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(canvas, x, y);
    lv_obj_set_size(canvas, width, height);
    lv_obj_set_style_border_width(canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(canvas, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(canvas, pixels, width, height, LV_IMG_CF_TRUE_COLOR);
    return canvas;
}

lv_obj_t *make_label_with_font(lv_obj_t *parent,
                               int x,
                               int y,
                               int w,
                               int h,
                               const char *text,
                               const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
    return label;
}

lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w, int h, const char *text)
{
    return make_label_with_font(parent, x, y, w, h, text, &zh_font_16);
}

void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if (!label || !text) {
        return;
    }
    const char *current = lv_label_get_text(label);
    if (!current || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

void draw_1bit_icon(lv_obj_t *canvas,
                    int width,
                    int height,
                    int bytes_per_row,
                    const uint8_t *bits,
                    lv_color_t foreground,
                    lv_color_t background)
{
    if (!canvas || !bits) {
        return;
    }
    lv_canvas_fill_bg(canvas, background, LV_OPA_COVER);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = bits + y * bytes_per_row;
        for (int x = 0; x < width; ++x) {
            if (row[x / 8] & (0x80 >> (x & 7))) {
                lv_canvas_set_px_color(canvas, x, y, foreground);
            }
        }
    }
    lv_obj_invalidate(canvas);
}

void canvas_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color)
{
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            lv_canvas_set_px_color(canvas, xx, yy, color);
        }
    }
}

void canvas_set_px_safe(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color)
{
    if (x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }
    lv_canvas_set_px_color(canvas, x, y, color);
}

void canvas_draw_line(lv_obj_t *canvas,
                      int w,
                      int h,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      lv_color_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        canvas_set_px_safe(canvas, x0, y0, w, h, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void canvas_draw_filled_circle(lv_obj_t *canvas,
                               int w,
                               int h,
                               int cx,
                               int cy,
                               int radius,
                               lv_color_t color)
{
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                canvas_set_px_safe(canvas, cx + x, cy + y, w, h, color);
            }
        }
    }
}

} // namespace sdl_preview_widgets
