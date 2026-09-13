// 声明天气时钟界面使用的 LVGL 字体资源。
#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(qweather_icons_36);
LV_FONT_DECLARE(zh_font_16);
LV_FONT_DECLARE(zh_flip_lunar_22);
LV_FONT_DECLARE(zh_pomodoro_title_24);

// 思源黑体 fallback 字体（4bpp，覆盖日文假名+韩文常用音节）
LV_FONT_DECLARE(lv_font_simsun_16_supplement);

// 共享 CJK fallback 链：zh_font_16 → supplement → montserrat
// 使用 RAM 副本构建 fallback 指针链，避免写 flash const。
const lv_font_t *get_cjk_font_with_supplement(void);
