#pragma once

#include <stdint.h>

// Demo 自用的 1-bit 字体描述，不再依赖整套 LVGL 字体引擎。
typedef struct {
    uint32_t bitmap_index;
    uint16_t adv_w;
    uint8_t box_w;
    uint8_t box_h;
    int8_t ofs_x;
    int8_t ofs_y;
} PowerGlyph;

typedef struct {
    const uint8_t *bitmap;
    const PowerGlyph *glyphs;
    const uint16_t *unicode_offsets;
    // 按当前字形、下一字形排列的 1/16 像素字距表；中文字体无需该表。
    const int8_t *kern_pairs;
    uint16_t glyph_count;
    uint16_t unicode_base;
    uint8_t line_height;
    uint8_t base_line;
} PowerFont;

#ifdef __cplusplus
extern "C" {
#endif

extern const PowerFont power_font_16;
extern const PowerFont power_font_24;
extern const PowerFont power_digits_48;
extern const PowerFont power_lunar_24;

#ifdef __cplusplus
}
#endif
