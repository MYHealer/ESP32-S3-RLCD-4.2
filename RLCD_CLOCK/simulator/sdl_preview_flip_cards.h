// 声明温湿时钟与小智预览共用的反显数字牌控件。
#pragma once

#include "lvgl.h"

namespace sdl_preview_flip_cards {

inline constexpr int kFlipCardW = 112;
inline constexpr int kFlipCardH = 112;

lv_obj_t *create_preview_flip_card(lv_obj_t *parent, int index, int x, int y);
void apply_preview_card_rounding(lv_obj_t *canvas);
void draw_preview_flip_card(lv_obj_t *canvas, int value);

} // namespace sdl_preview_flip_cards
