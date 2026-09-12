// 提供 LVGL canvas 缓冲分配、安全打点、线段、虚线和圆点等基础工具。
#include "ui_canvas_primitives.h"

#include "app_metadata.h"
#include "checked_size.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include <cstdlib>

#define UI_CANVAS_BUFFER_INVALID_SIZE_FORMAT "canvas buffer invalid size %dx%d"
#define UI_CANVAS_BUFFER_SIZE_OVERFLOW_FORMAT "canvas buffer size overflow %dx%d"
#define UI_CANVAS_BUFFER_ALLOC_FAILED_FORMAT "canvas buffer alloc failed %dx%d"

namespace {
constexpr int kDashedLineRunPixels = 5;
constexpr int kDashedLinePeriodPixels = kDashedLineRunPixels * 2;
constexpr int kBresenhamErrorScale = 2;
constexpr size_t kCanvasInternalFallbackMaxBytes = 12U * 1024U;

static_assert(kDashedLineRunPixels > 0, "dashed line run length must be positive");
static_assert(kDashedLinePeriodPixels > kDashedLineRunPixels,
              "dashed line period must be longer than the drawn segment");
static_assert(kBresenhamErrorScale == 2, "Bresenham error scale must stay doubled");

constexpr bool canvas_internal_fallback_allowed(size_t buffer_bytes)
{
    return buffer_bytes > 0 &&
           buffer_bytes <= kCanvasInternalFallbackMaxBytes;
}

static_assert(canvas_internal_fallback_allowed(1),
              "small canvas buffers must retain an internal-RAM fallback");
static_assert(!canvas_internal_fallback_allowed(
                  kCanvasInternalFallbackMaxBytes + 1),
              "large canvas buffers must remain PSRAM-only");

bool canvas_size_valid(int w, int h)
{
    return w > 0 && h > 0;
}

bool canvas_point_in_bounds(int x, int y, int w, int h)
{
    return canvas_size_valid(w, h) && x >= 0 && y >= 0 && x < w && y < h;
}

bool canvas_y_in_bounds(int y, int w, int h)
{
    return canvas_size_valid(w, h) && y >= 0 && y < h;
}

void order_int_pair(int *first, int *second)
{
    if (!first || !second || *first <= *second) {
        return;
    }
    int tmp = *first;
    *first = *second;
    *second = tmp;
}

int abs_delta(int start, int end)
{
    return end > start ? end - start : start - end;
}

int line_step(int start, int end)
{
    return start < end ? 1 : -1;
}

int square_int(int value)
{
    return value * value;
}

bool canvas_pixel_count(int width,
                        int height,
                        size_t *pixel_count,
                        size_t *buffer_bytes)
{
    if (!pixel_count || !buffer_bytes) {
        return false;
    }
    *pixel_count = 0;
    *buffer_bytes = 0;
    if (width <= 0 || height <= 0) {
        ESP_LOGW(TAG, UI_CANVAS_BUFFER_INVALID_SIZE_FORMAT, width, height);
        return false;
    }
    size_t count = 0;
    size_t bytes = 0;
    if (!app_memory::checked_size_multiply(static_cast<size_t>(width),
                                           static_cast<size_t>(height),
                                           &count) ||
        !app_memory::checked_size_multiply(count, sizeof(lv_color_t), &bytes)) {
        ESP_LOGW(TAG, UI_CANVAS_BUFFER_SIZE_OVERFLOW_FORMAT, width, height);
        return false;
    }
    *pixel_count = count;
    *buffer_bytes = bytes;
    return true;
}
} // namespace

lv_color_t *alloc_canvas_buffer(int width, int height)
{
    size_t pixel_count = 0;
    size_t buffer_bytes = 0;
    if (!canvas_pixel_count(width,
                            height,
                            &pixel_count,
                            &buffer_bytes)) {
        return nullptr;
    }
    lv_color_t *buf = (lv_color_t *)heap_caps_calloc(pixel_count,
                                                     sizeof(lv_color_t),
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf && canvas_internal_fallback_allowed(buffer_bytes)) {
        buf = (lv_color_t *)calloc(pixel_count, sizeof(lv_color_t));
    }
    if (!buf) {
        ESP_LOGW(TAG, UI_CANVAS_BUFFER_ALLOC_FAILED_FORMAT, width, height);
    }
    return buf;
}

bool ensure_canvas_buffer(lv_color_t **buffer, int width, int height)
{
    if (!buffer) {
        return false;
    }
    if (!*buffer) {
        *buffer = alloc_canvas_buffer(width, height);
    }
    return *buffer != nullptr;
}

void configure_canvas_base(lv_obj_t *canvas,
                           lv_color_t *buffer,
                           int x,
                           int y,
                           int width,
                           int height)
{
    if (!canvas || !buffer || !canvas_size_valid(width, height)) {
        return;
    }
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(canvas, x, y);
    lv_obj_set_size(canvas, width, height);
    lv_obj_set_style_border_width(canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(canvas, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(canvas, buffer, width, height, LV_IMG_CF_TRUE_COLOR);
}

int clamp_int(int value, int min_value, int max_value)
{
    order_int_pair(&min_value, &max_value);
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

void invalidate_canvas_rect(lv_obj_t *canvas, int x1, int y1, int x2, int y2)
{
    if (!canvas) {
        return;
    }
    order_int_pair(&x1, &x2);
    order_int_pair(&y1, &y2);

    lv_area_t canvas_coords = {};
    lv_obj_get_coords(canvas, &canvas_coords);
    lv_area_t area = {};
    area.x1 = static_cast<lv_coord_t>(canvas_coords.x1 + x1);
    area.y1 = static_cast<lv_coord_t>(canvas_coords.y1 + y1);
    area.x2 = static_cast<lv_coord_t>(canvas_coords.x1 + x2);
    area.y2 = static_cast<lv_coord_t>(canvas_coords.y1 + y2);
    lv_obj_invalidate_area(canvas, &area);
}

void canvas_image_set_px_safe(lv_img_dsc_t *image,
                              int x,
                              int y,
                              int w,
                              int h,
                              lv_color_t color)
{
    if (!image ||
        !canvas_point_in_bounds(x, y, w, h) ||
        x >= image->header.w ||
        y >= image->header.h) {
        return;
    }
    lv_img_buf_set_px_color(image, x, y, color);
}

void canvas_draw_line(lv_obj_t *canvas, int w, int h, int x0, int y0, int x1, int y1, lv_color_t color)
{
    if (!canvas || !canvas_size_valid(w, h)) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    int dx = abs_delta(x0, x1);
    int sx = line_step(x0, x1);
    int dy = -abs_delta(y0, y1);
    int sy = line_step(y0, y1);
    int err = dx + dy;
    for (;;) {
        canvas_image_set_px_safe(image, x0, y0, w, h, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = kBresenhamErrorScale * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void canvas_draw_dashed_hline(lv_obj_t *canvas, int w, int h, int x1, int x2, int y, lv_color_t color)
{
    if (!canvas || !canvas_y_in_bounds(y, w, h)) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    order_int_pair(&x1, &x2);
    if (x2 < 0 || x1 >= w) {
        return;
    }
    int draw_x1 = clamp_int(x1, 0, w - 1);
    int draw_x2 = clamp_int(x2, 0, w - 1);
    for (int x = draw_x1; x <= draw_x2; ++x) {
        if (((x - x1) % kDashedLinePeriodPixels) < kDashedLineRunPixels) {
            canvas_image_set_px_safe(image, x, y, w, h, color);
        }
    }
}

void canvas_draw_filled_circle(lv_obj_t *canvas, int w, int h, int cx, int cy, int radius, lv_color_t color)
{
    if (!canvas || !canvas_size_valid(w, h) || radius < 0) {
        return;
    }
    if (cx + radius < 0 || cx - radius >= w || cy + radius < 0 || cy - radius >= h) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    const int radius_squared = square_int(radius);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (square_int(x) + square_int(y) <= radius_squared) {
                canvas_image_set_px_safe(image, cx + x, cy + y, w, h, color);
            }
        }
    }
}
