// 声明温湿时钟与小智共用的反显 DSEG 数字牌像素绘制接口。
#pragma once

#include "lvgl.h"

#include <time.h>

namespace inverted_clock_card {

inline constexpr int kCount = 3;
inline constexpr int kWidth = 112;
inline constexpr int kHeight = 112;
inline constexpr int kY = 66;
inline constexpr int kX[kCount] = {18, 144, 270};

void render(lv_obj_t *canvas, int value);
void clear(lv_obj_t *canvas);

} // namespace inverted_clock_card

void build_inverted_clock_cards(lv_obj_t *parent,
                                lv_obj_t *card_canvas[3],
                                lv_color_t *card_canvas_buf[3]);
bool update_inverted_clock_cards(const struct tm &local,
                                 lv_obj_t *const card_canvas[3],
                                 int last_values[3]);
void clear_inverted_clock_card(lv_obj_t *card_canvas);
bool update_inverted_clock_card_value(lv_obj_t *card_canvas,
                                      int value,
                                      int *last_value);
