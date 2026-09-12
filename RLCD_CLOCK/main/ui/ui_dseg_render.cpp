// 实现固件与 SDL 预览共用的 DSEG 字形查找和像素绘制。
#include "ui_dseg_render.h"

#include "dseg_digits.h"

#include <string.h>

namespace {
constexpr uint32_t kBitsPerByte = 8;
constexpr uint8_t kPackedBitMask = 0x80;

bool packed_bit_is_set(const uint8_t *bits, uint32_t bit_index)
{
    if (!bits) {
        return false;
    }
    return (bits[bit_index / kBitsPerByte] &
            static_cast<uint8_t>(kPackedBitMask >> (bit_index % kBitsPerByte))) != 0;
}
} // namespace

const DsegGlyph *find_dseg_glyph(const DsegFont &font, char ch)
{
    if (!font.chars || !font.glyphs) {
        return nullptr;
    }
    const char *pos = strchr(font.chars, ch);
    if (!pos) {
        return nullptr;
    }
    return &font.glyphs[pos - font.chars];
}

int draw_dseg_text(lv_obj_t *canvas,
                   const DsegFont &font,
                   const char *text,
                   int cursor_x,
                   int baseline_y)
{
    int x_cursor = cursor_x;
    if (!canvas || !text || !font.bitmap) {
        return x_cursor;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return x_cursor;
    }
    for (const char *p = text; *p; ++p) {
        const DsegGlyph *glyph = find_dseg_glyph(font, *p);
        if (!glyph) {
            continue;
        }
        uint32_t bit = 0;
        const uint8_t *glyph_bitmap = font.bitmap + glyph->bitmap_offset;
        for (int y = 0; y < glyph->height; ++y) {
            for (int x = 0; x < glyph->width; ++x, ++bit) {
                if (packed_bit_is_set(glyph_bitmap, bit)) {
                    lv_img_buf_set_px_color(image,
                                            x_cursor + glyph->x_offset + x,
                                            baseline_y + glyph->y_offset + y,
                                            lv_color_black());
                }
            }
        }
        x_cursor += glyph->x_advance;
    }
    return x_cursor;
}
