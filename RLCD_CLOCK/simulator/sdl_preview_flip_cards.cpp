// 实现温湿时钟与小智预览共用的反显数字牌创建和绘制。
#include "sdl_preview_flip_cards.h"

#include <vector>

#include "dseg_digits.h"
#include "ui/ui_dseg_layout.h"
#include "ui/ui_dseg_render.h"

namespace sdl_preview_flip_cards {
namespace {

constexpr int kFlipCardCount = 3;
constexpr int kFlipCardRadius = 8;
std::vector<lv_color_t> g_flip_card_pixels[kFlipCardCount] = {
    std::vector<lv_color_t>(kFlipCardW * kFlipCardH),
    std::vector<lv_color_t>(kFlipCardW * kFlipCardH),
    std::vector<lv_color_t>(kFlipCardW * kFlipCardH),
};

} // namespace

lv_obj_t *create_preview_flip_card(lv_obj_t *parent, int index, int x, int y)
{
    if (!parent || index < 0 || index >= kFlipCardCount) {
        return nullptr;
    }
    lv_obj_t *card = lv_canvas_create(parent);
    if (!card) {
        return nullptr;
    }
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, kFlipCardW, kFlipCardH);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(card,
                         g_flip_card_pixels[index].data(),
                         kFlipCardW,
                         kFlipCardH,
                         LV_IMG_CF_TRUE_COLOR);
    return card;
}

void apply_preview_card_rounding(lv_obj_t *canvas)
{
    if (!canvas) {
        return;
    }
    int radius = kFlipCardRadius;
    int r2 = radius * radius;
    for (int y = 0; y < radius; ++y) {
        for (int x = 0; x < radius; ++x) {
            int dx = radius - 1 - x;
            int dy = radius - 1 - y;
            if (dx * dx + dy * dy > r2) {
                lv_canvas_set_px_color(canvas, x, y, lv_color_white());
                lv_canvas_set_px_color(canvas, kFlipCardW - 1 - x, y, lv_color_white());
                lv_canvas_set_px_color(canvas, x, kFlipCardH - 1 - y, lv_color_white());
                lv_canvas_set_px_color(canvas,
                                       kFlipCardW - 1 - x,
                                       kFlipCardH - 1 - y,
                                       lv_color_white());
            }
        }
    }
}

void draw_preview_flip_card(lv_obj_t *canvas, int value)
{
    if (!canvas) {
        return;
    }
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    constexpr int scale_num = 3;
    constexpr int scale_den = 4;
    const DsegGlyph *tens = find_dseg_glyph(kDSEG84Font, static_cast<char>('0' + value / 10));
    const DsegGlyph *ones = find_dseg_glyph(kDSEG84Font, static_cast<char>('0' + value % 10));
    if (!tens || !ones) {
        return;
    }
    auto draw_scaled = [&](const DsegGlyph *glyph, int origin_x, int origin_y) {
        int dst_w = (glyph->width * scale_num + scale_den - 1) / scale_den;
        int dst_h = (glyph->height * scale_num + scale_den - 1) / scale_den;
        int dst_x = origin_x + (glyph->x_offset * scale_num) / scale_den;
        int dst_y = origin_y + (glyph->y_offset * scale_num) / scale_den;
        for (int yy = 0; yy < dst_h; ++yy) {
            int src_y = (yy * glyph->height) / dst_h;
            for (int xx = 0; xx < dst_w; ++xx) {
                int src_x = (xx * glyph->width) / dst_w;
                uint32_t bit = static_cast<uint32_t>(src_y) * glyph->width + src_x;
                uint8_t byte = kDSEG84Font.bitmap[glyph->bitmap_offset + bit / 8];
                if (byte & (0x80 >> (bit & 7))) {
                    lv_canvas_set_px_color(canvas, dst_x + xx, dst_y + yy, lv_color_white());
                }
            }
        }
    };
    DsegPairLayout layout = centered_dseg_pair_layout(kFlipCardW,
                                                       scale_num,
                                                       scale_den,
                                                       tens->x_offset,
                                                       tens->width,
                                                       tens->x_advance,
                                                       ones->x_offset,
                                                       ones->width);
    int baseline_y = 84;
    draw_scaled(tens, layout.first_origin_x, baseline_y);
    draw_scaled(ones, layout.second_origin_x, baseline_y);
    apply_preview_card_rounding(canvas);
    lv_obj_invalidate(canvas);
}

} // namespace sdl_preview_flip_cards
