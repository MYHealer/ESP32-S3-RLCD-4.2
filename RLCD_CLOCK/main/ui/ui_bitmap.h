// 声明 packed 1-bit 位图、图标和趋势箭头的共享绘制接口。
#pragma once

#include "lvgl.h"

#include <stdint.h>

bool packed_1bit_bit_is_set(const uint8_t *bits, uint32_t bit_index);
void draw_1bit_icon(lv_obj_t *canvas,
                    int width,
                    int height,
                    int bytes_per_row,
                    const uint8_t *bits,
                    lv_color_t fg,
                    lv_color_t bg);
void draw_1bit_icon_centered(lv_obj_t *canvas,
                             int width,
                             int height,
                             int bytes_per_row,
                             const uint8_t *bits,
                             lv_color_t fg,
                             lv_color_t bg);
bool update_trend_icon(lv_obj_t *canvas, int trend, int *last_trend);
