// 声明固件与 SDL 预览共用的 DSEG 字形查找和文本绘制接口。
#pragma once

#include "lvgl.h"

struct DsegGlyph;
struct DsegFont;

const DsegGlyph *find_dseg_glyph(const DsegFont &font, char ch);
int draw_dseg_text(lv_obj_t *canvas,
                   const DsegFont &font,
                   const char *text,
                   int cursor_x,
                   int baseline_y);
