// 提供 SDL 预览页面共用的基础 LVGL 标签和黑白条控件。
#pragma once

#include "lvgl.h"

namespace sdl_preview_widgets {

void set_obj_black(lv_obj_t *obj, bool active);
lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h);
lv_obj_t *make_black_bar(lv_obj_t *parent, int x, int y, int w, int h);
lv_obj_t *make_canvas(lv_obj_t *parent,
                      int x,
                      int y,
                      int width,
                      int height,
                      lv_color_t *pixels);
lv_obj_t *make_label_with_font(lv_obj_t *parent,
                               int x,
                               int y,
                               int w,
                               int h,
                               const char *text,
                               const lv_font_t *font);
lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w, int h, const char *text);
void set_label_text_if_changed(lv_obj_t *label, const char *text);
void draw_1bit_icon(lv_obj_t *canvas,
                    int width,
                    int height,
                    int bytes_per_row,
                    const uint8_t *bits,
                    lv_color_t foreground,
                    lv_color_t background);
void canvas_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color);
void canvas_set_px_safe(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color);
void canvas_draw_line(lv_obj_t *canvas,
                      int w,
                      int h,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      lv_color_t color);
void canvas_draw_filled_circle(lv_obj_t *canvas,
                               int w,
                               int h,
                               int cx,
                               int cy,
                               int radius,
                               lv_color_t color);

} // namespace sdl_preview_widgets
