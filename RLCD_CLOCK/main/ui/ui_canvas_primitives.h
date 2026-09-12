// 声明页面 canvas 复用的缓冲、配置、局部失效与基础绘制图元。
#pragma once

#include "lvgl.h"

lv_color_t *alloc_canvas_buffer(int width, int height);
bool ensure_canvas_buffer(lv_color_t **buffer, int width, int height);
void configure_canvas_base(lv_obj_t *canvas,
                           lv_color_t *buffer,
                           int x,
                           int y,
                           int width,
                           int height);
int clamp_int(int value, int min_value, int max_value);
void invalidate_canvas_rect(lv_obj_t *canvas, int x1, int y1, int x2, int y2);
void canvas_image_set_px_safe(lv_img_dsc_t *image,
                              int x,
                              int y,
                              int width,
                              int height,
                              lv_color_t color);
void canvas_draw_line(lv_obj_t *canvas,
                      int width,
                      int height,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      lv_color_t color);
void canvas_draw_dashed_hline(lv_obj_t *canvas,
                              int width,
                              int height,
                              int x1,
                              int x2,
                              int y,
                              lv_color_t color);
void canvas_draw_filled_circle(lv_obj_t *canvas,
                               int width,
                               int height,
                               int cx,
                               int cy,
                               int radius,
                               lv_color_t color);
