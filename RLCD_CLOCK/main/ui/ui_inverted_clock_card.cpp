// 绘制反显 DSEG 数字牌，并按变化字形的包围框局部失效刷新。
#include "ui_inverted_clock_card.h"

#include "app_display_config.h"
#include "app_metadata.h"
#include "dseg_digits.h"
#include "ui_bitmap.h"
#include "ui_canvas_primitives.h"
#include "ui_dseg_layout.h"
#include "ui_dseg_render.h"

#include <esp_log.h>

#define INVERTED_CLOCK_CARD_CANVAS_CREATE_FAILED_FORMAT "flip clock card %d canvas create failed"

namespace inverted_clock_card {
namespace {

constexpr int kRadius = 8;
constexpr int kDigitScaleNumerator = 3;
constexpr int kDigitScaleDenominator = 4;
constexpr int kDigitBaselineY = 84;
constexpr int kPairDigitCount = 2;

struct DigitPlacement {
    const DsegGlyph *glyph;
    int origin_x;
    DsegGlyphBounds bounds;
};

struct PairPlacement {
    DigitPlacement digits[kPairDigitCount];
};

static_assert(kRadius > 0 && kRadius * 2 <= kWidth && kRadius * 2 <= kHeight,
              "inverted clock card radius must fit card");
static_assert(kDigitScaleNumerator > 0 && kDigitScaleDenominator > 0,
              "inverted clock digit scale must be positive");
static_assert(kCount == 3, "inverted clock card group must keep three cards");
static_assert(kX[0] >= 0 && kX[kCount - 1] + kWidth <= kDisplayWidth,
              "inverted clock card group must fit display width");
static_assert(kY >= 0 && kY + kHeight <= kDisplayHeight,
              "inverted clock card group must fit display height");

void set_buffer_px_safe(lv_img_dsc_t *image, int x, int y, lv_color_t color)
{
    if (!image || x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return;
    }
    lv_img_buf_set_px_color(image, x, y, color);
}

void apply_rounding(lv_img_dsc_t *image)
{
    if (!image) {
        return;
    }
    int r2 = kRadius * kRadius;
    for (int y = 0; y < kRadius; ++y) {
        for (int x = 0; x < kRadius; ++x) {
            int dx = kRadius - 1 - x;
            int dy = kRadius - 1 - y;
            if (dx * dx + dy * dy > r2) {
                set_buffer_px_safe(image, x, y, lv_color_white());
                set_buffer_px_safe(image, kWidth - 1 - x, y, lv_color_white());
                set_buffer_px_safe(image, x, kHeight - 1 - y, lv_color_white());
                set_buffer_px_safe(image,
                                   kWidth - 1 - x,
                                   kHeight - 1 - y,
                                   lv_color_white());
            }
        }
    }
}

bool dseg_pixel_on(const DsegFont &font, const DsegGlyph *glyph, int x, int y)
{
    uint32_t bit = static_cast<uint32_t>(y) * glyph->width + x;
    return packed_1bit_bit_is_set(font.bitmap + glyph->bitmap_offset, bit);
}

DigitPlacement digit_placement(const DsegGlyph *glyph, int origin_x)
{
    if (!glyph) {
        return {nullptr, origin_x, {0, 0, -1, -1}};
    }
    return {
        glyph,
        origin_x,
        dseg_scaled_glyph_bounds(origin_x,
                                 kDigitBaselineY,
                                 kDigitScaleNumerator,
                                 kDigitScaleDenominator,
                                 glyph->x_offset,
                                 glyph->y_offset,
                                 glyph->width,
                                 glyph->height),
    };
}

bool pair_placement(int value, PairPlacement *placement)
{
    if (!placement || value < 0 || value > 99) {
        return false;
    }
    const DsegGlyph *tens = find_dseg_glyph(kDSEG84Font,
                                            static_cast<char>('0' + value / 10));
    const DsegGlyph *ones = find_dseg_glyph(kDSEG84Font,
                                            static_cast<char>('0' + value % 10));
    if (!tens || !ones) {
        return false;
    }
    const DsegPairLayout layout = centered_dseg_pair_layout(kWidth,
                                                            kDigitScaleNumerator,
                                                            kDigitScaleDenominator,
                                                            tens->x_offset,
                                                            tens->width,
                                                            tens->x_advance,
                                                            ones->x_offset,
                                                            ones->width);
    placement->digits[0] = digit_placement(tens, layout.first_origin_x);
    placement->digits[1] = digit_placement(ones, layout.second_origin_x);
    return true;
}

void draw_scaled_dseg_digit(lv_img_dsc_t *image,
                            const DsegGlyph *glyph,
                            int origin_x,
                            int origin_y,
                            int scale_num,
                            int scale_den,
                            int clip_y0 = 0,
                            int clip_y1 = kHeight)
{
    if (!image || !glyph || scale_num <= 0 || scale_den <= 0) {
        return;
    }
    int dst_w = (glyph->width * scale_num + scale_den - 1) / scale_den;
    int dst_h = (glyph->height * scale_num + scale_den - 1) / scale_den;
    int dst_x = origin_x + (glyph->x_offset * scale_num) / scale_den;
    int dst_y = origin_y + (glyph->y_offset * scale_num) / scale_den;
    for (int y = 0; y < dst_h; ++y) {
        int src_y = (y * glyph->height) / dst_h;
        for (int x = 0; x < dst_w; ++x) {
            int src_x = (x * glyph->width) / dst_w;
            int py = dst_y + y;
            if (py >= clip_y0 && py < clip_y1 &&
                dseg_pixel_on(kDSEG84Font, glyph, src_x, src_y)) {
                set_buffer_px_safe(image,
                                   dst_x + x,
                                   dst_y + y,
                                   lv_color_white());
            }
        }
    }
}

void draw_digits(lv_img_dsc_t *image,
                 int value,
                 int clip_y0 = 0,
                 int clip_y1 = kHeight)
{
    PairPlacement placement = {};
    if (!pair_placement(value, &placement)) {
        return;
    }
    for (const DigitPlacement &digit : placement.digits) {
        draw_scaled_dseg_digit(image,
                               digit.glyph,
                               digit.origin_x,
                               kDigitBaselineY,
                               kDigitScaleNumerator,
                               kDigitScaleDenominator,
                               clip_y0,
                               clip_y1);
    }
}

void draw_shell(lv_obj_t *canvas)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
}

bool digit_placement_equal(const DigitPlacement &left,
                           const DigitPlacement &right)
{
    return left.glyph == right.glyph &&
           left.origin_x == right.origin_x &&
           dseg_glyph_bounds_equal(left.bounds, right.bounds);
}

void fill_digit_bounds(lv_img_dsc_t *image,
                       const DsegGlyphBounds &bounds,
                       lv_color_t color)
{
    if (!image || !dseg_glyph_bounds_valid(bounds)) {
        return;
    }
    for (int y = bounds.y1; y <= bounds.y2; ++y) {
        for (int x = bounds.x1; x <= bounds.x2; ++x) {
            set_buffer_px_safe(image, x, y, color);
        }
    }
}

void render_transition(lv_obj_t *canvas, int previous_value, int next_value)
{
    lv_img_dsc_t *image = canvas ? lv_canvas_get_img(canvas) : nullptr;
    if (!image) {
        return;
    }
    PairPlacement previous = {};
    PairPlacement next = {};
    if (!pair_placement(previous_value, &previous) ||
        !pair_placement(next_value, &next)) {
        draw_shell(canvas);
        draw_digits(image, next_value);
        apply_rounding(image);
        lv_obj_invalidate(canvas);
        return;
    }

    bool dirty[kPairDigitCount] = {};
    DsegGlyphBounds dirty_bounds[kPairDigitCount] = {};
    for (int i = 0; i < kPairDigitCount; ++i) {
        dirty[i] = !digit_placement_equal(previous.digits[i], next.digits[i]);
        dirty_bounds[i] = dseg_union_glyph_bounds(previous.digits[i].bounds,
                                                  next.digits[i].bounds);
    }
    for (int i = 0; i < kPairDigitCount; ++i) {
        if (!dirty[i]) {
            continue;
        }
        for (int j = 0; j < kPairDigitCount; ++j) {
            if (!dirty[j] &&
                (dseg_glyph_bounds_overlap(dirty_bounds[i], previous.digits[j].bounds) ||
                 dseg_glyph_bounds_overlap(dirty_bounds[i], next.digits[j].bounds))) {
                dirty[j] = true;
            }
        }
    }
    for (int i = 0; i < kPairDigitCount; ++i) {
        if (dirty[i]) {
            fill_digit_bounds(image, dirty_bounds[i], lv_color_black());
        }
    }
    for (int i = 0; i < kPairDigitCount; ++i) {
        if (!dirty[i]) {
            continue;
        }
        draw_scaled_dseg_digit(image,
                               next.digits[i].glyph,
                               next.digits[i].origin_x,
                               kDigitBaselineY,
                               kDigitScaleNumerator,
                               kDigitScaleDenominator);
    }
    for (int i = 0; i < kPairDigitCount; ++i) {
        if (dirty[i]) {
            invalidate_canvas_rect(canvas,
                                   dirty_bounds[i].x1,
                                   dirty_bounds[i].y1,
                                   dirty_bounds[i].x2,
                                   dirty_bounds[i].y2);
        }
    }
}

} // namespace

void render(lv_obj_t *canvas, int value)
{
    if (!canvas) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    draw_shell(canvas);
    draw_digits(image, value);
    apply_rounding(image);
    lv_obj_invalidate(canvas);
}

void clear(lv_obj_t *canvas)
{
    if (!canvas) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    draw_shell(canvas);
    apply_rounding(image);
    lv_obj_invalidate(canvas);
}

} // namespace inverted_clock_card

void build_inverted_clock_cards(lv_obj_t *parent,
                                lv_obj_t *card_canvas[3],
                                lv_color_t *card_canvas_buf[3])
{
    if (!parent || !card_canvas || !card_canvas_buf) {
        return;
    }
    for (int i = 0; i < inverted_clock_card::kCount; ++i) {
        if (!card_canvas_buf[i]) {
            card_canvas_buf[i] = alloc_canvas_buffer(inverted_clock_card::kWidth,
                                                     inverted_clock_card::kHeight);
        }
        if (!card_canvas_buf[i]) {
            continue;
        }
        card_canvas[i] = lv_canvas_create(parent);
        if (!card_canvas[i]) {
            ESP_LOGW(TAG, INVERTED_CLOCK_CARD_CANVAS_CREATE_FAILED_FORMAT, i);
            continue;
        }
        configure_canvas_base(card_canvas[i],
                              card_canvas_buf[i],
                              inverted_clock_card::kX[i],
                              inverted_clock_card::kY,
                              inverted_clock_card::kWidth,
                              inverted_clock_card::kHeight);
        lv_canvas_fill_bg(card_canvas[i], lv_color_black(), LV_OPA_COVER);
    }
}

bool update_inverted_clock_cards(const struct tm &local,
                                 lv_obj_t *const card_canvas[3],
                                 int last_values[3])
{
    if (!card_canvas || !last_values) {
        return false;
    }
    const int values[inverted_clock_card::kCount] = {
        local.tm_hour,
        local.tm_min,
        local.tm_sec,
    };
    bool changed = false;
    for (int i = 0; i < inverted_clock_card::kCount; ++i) {
        changed |= update_inverted_clock_card_value(card_canvas[i],
                                                     values[i],
                                                     &last_values[i]);
    }
    return changed;
}

void clear_inverted_clock_card(lv_obj_t *card_canvas)
{
    inverted_clock_card::clear(card_canvas);
}

bool update_inverted_clock_card_value(lv_obj_t *card_canvas,
                                      int value,
                                      int *last_value)
{
    if (!card_canvas || !last_value || value < 0 || value > 99 || value == *last_value) {
        return false;
    }
    const int previous_value = *last_value;
    if (previous_value >= 0 && previous_value <= 99) {
        inverted_clock_card::render_transition(card_canvas,
                                               previous_value,
                                               value);
    } else {
        inverted_clock_card::render(card_canvas, value);
    }
    *last_value = value;
    return true;
}
