// 读取 packed 1-bit 数据并绘制通用图标和温湿度趋势箭头。
#include "ui_bitmap.h"

#include "app_metadata.h"
#include "ui_icons.h"

#include <esp_log.h>

#define UI_1BIT_ICON_INVALID_SIZE_FORMAT "1bit icon invalid size %dx%d row=%d"
#define UI_1BIT_ICON_ROW_TOO_SMALL_FORMAT "1bit icon row too small width=%d row=%d min=%d"
#define UI_1BIT_ICON_CANVAS_TOO_SMALL_FORMAT \
    "1bit icon canvas too small canvas=%dx%d icon=%dx%d"

namespace {
constexpr int kBitsPerByte = 8;
constexpr uint8_t kPacked1BitMsbMask = 0x80;

int packed_1bit_bytes_per_row(int width)
{
    return (width + kBitsPerByte - 1) / kBitsPerByte;
}

bool icon_draw_args_valid(lv_obj_t *canvas, const uint8_t *bits)
{
    return canvas && bits;
}

bool draw_1bit_icon_pixels(lv_img_dsc_t *image,
                           int offset_x,
                           int offset_y,
                           int width,
                           int height,
                           int bytes_per_row,
                           const uint8_t *bits,
                           lv_color_t fg)
{
    if (!image || !bits ||
        offset_x < 0 || offset_y < 0 ||
        offset_x + width > image->header.w ||
        offset_y + height > image->header.h) {
        return false;
    }
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = bits + y * bytes_per_row;
        for (int x = 0; x < width; ++x) {
            if (packed_1bit_bit_is_set(row, static_cast<uint32_t>(x))) {
                lv_img_buf_set_px_color(image, offset_x + x, offset_y + y, fg);
            }
        }
    }
    return true;
}
}

bool packed_1bit_bit_is_set(const uint8_t *bits, uint32_t bit_index)
{
    if (!bits) {
        return false;
    }
    return bits[bit_index / kBitsPerByte] & (kPacked1BitMsbMask >> (bit_index & (kBitsPerByte - 1)));
}

void draw_1bit_icon(lv_obj_t *canvas,
                    int width,
                    int height,
                    int bytes_per_row,
                    const uint8_t *bits,
                    lv_color_t fg,
                    lv_color_t bg)
{
    if (!icon_draw_args_valid(canvas, bits)) {
        return;
    }
    if (width <= 0 || height <= 0 || bytes_per_row <= 0) {
        ESP_LOGW(TAG, UI_1BIT_ICON_INVALID_SIZE_FORMAT, width, height, bytes_per_row);
        return;
    }
    int min_bytes_per_row = packed_1bit_bytes_per_row(width);
    if (bytes_per_row < min_bytes_per_row) {
        ESP_LOGW(TAG, UI_1BIT_ICON_ROW_TOO_SMALL_FORMAT, width, bytes_per_row, min_bytes_per_row);
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    if (!draw_1bit_icon_pixels(image,
                               0,
                               0,
                               width,
                               height,
                               bytes_per_row,
                               bits,
                               fg)) {
        ESP_LOGW(TAG,
                 UI_1BIT_ICON_CANVAS_TOO_SMALL_FORMAT,
                 image->header.w,
                 image->header.h,
                 width,
                 height);
    }
    lv_obj_invalidate(canvas);
}

void draw_1bit_icon_centered(lv_obj_t *canvas,
                             int width,
                             int height,
                             int bytes_per_row,
                             const uint8_t *bits,
                             lv_color_t fg,
                             lv_color_t bg)
{
    if (!icon_draw_args_valid(canvas, bits)) {
        return;
    }
    if (width <= 0 || height <= 0 || bytes_per_row <= 0) {
        ESP_LOGW(TAG, UI_1BIT_ICON_INVALID_SIZE_FORMAT, width, height, bytes_per_row);
        return;
    }
    const int min_bytes_per_row = packed_1bit_bytes_per_row(width);
    if (bytes_per_row < min_bytes_per_row) {
        ESP_LOGW(TAG, UI_1BIT_ICON_ROW_TOO_SMALL_FORMAT, width, bytes_per_row, min_bytes_per_row);
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    const int offset_x = (static_cast<int>(image->header.w) - width) / 2;
    const int offset_y = (static_cast<int>(image->header.h) - height) / 2;
    if (!draw_1bit_icon_pixels(image,
                               offset_x,
                               offset_y,
                               width,
                               height,
                               bytes_per_row,
                               bits,
                               fg)) {
        ESP_LOGW(TAG,
                 UI_1BIT_ICON_CANVAS_TOO_SMALL_FORMAT,
                 image->header.w,
                 image->header.h,
                 width,
                 height);
    }
    lv_obj_invalidate(canvas);
}

bool update_trend_icon(lv_obj_t *canvas, int trend, int *last_trend)
{
    if (!canvas) {
        return false;
    }
    if (last_trend && *last_trend == trend) {
        return false;
    }
    const uint8_t *bits = nullptr;
    if (trend > 0) {
        bits = trend_up_icon_bits;
    } else if (trend < 0) {
        bits = trend_down_icon_bits;
    }
    if (bits) {
        draw_1bit_icon(canvas,
                       TREND_ICON_WIDTH,
                       TREND_ICON_HEIGHT,
                       TREND_ICON_BYTES_PER_ROW,
                       bits,
                       lv_color_black(),
                       lv_color_white());
    } else {
        lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
        lv_obj_invalidate(canvas);
    }
    if (last_trend) {
        *last_trend = trend;
    }
    return true;
}
