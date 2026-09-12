// 构建 60 段工作页进度条，并按填充变化执行局部刷新。
#include "ui_progress.h"

#include "app_metadata.h"
#include "ui_canvas_primitives.h"
#include "ui_clock_time.h"
#include "ui_page_state.h"
#include "work_page_ids.h"

#include <esp_attr.h>
#include <esp_log.h>

#define UI_PROGRESS_CANVAS_BUILD_INVALID_ARG_LOG "progress canvas build invalid arg"
#define UI_PROGRESS_CANVAS_CREATE_FAILED_LOG "progress canvas create failed"

namespace {
constexpr int kProgressSegmentCount = 60;
constexpr int kProgressSegmentW = 5;
constexpr int kProgressSegmentH = 3;
constexpr int kProgressSegmentGap = 1;
constexpr int kProgressSegmentStride = kProgressSegmentW + kProgressSegmentGap;
constexpr int kProgressCanvasX = 20;
constexpr int kProgressCanvasW = kProgressSegmentCount * kProgressSegmentStride - kProgressSegmentGap;
constexpr int kProgressCanvasH = kProgressSegmentH;
constexpr int kWorkPageDayProgressY = 59;
static_assert(kWorkPageCount > 0, "work page count must be positive");

EXT_RAM_BSS_ATTR lv_obj_t *s_work_page_day_progress_canvas[kWorkPageCount] = {};
// All work pages display identical pixels, so one PSRAM buffer is sufficient.
lv_color_t *s_work_page_day_progress_buffer = nullptr;
int s_work_page_day_progress_filled = -1;

bool is_progress_segment_index(int index)
{
    return index >= 0 && index < kProgressSegmentCount;
}

int clamp_progress_filled_count(int filled)
{
    if (filled < 0) {
        return 0;
    }
    if (filled > kProgressSegmentCount) {
        return kProgressSegmentCount;
    }
    return filled;
}

bool is_progress_segment_border_pixel(int x, int y)
{
    return x == 0 || x == kProgressSegmentW - 1 ||
           y == 0 || y == kProgressSegmentH - 1;
}

bool progress_update_args_valid(lv_obj_t *canvas, const int *last_filled)
{
    return canvas && last_filled;
}

void draw_progress_segment(lv_obj_t *canvas, int index, bool filled)
{
    if (!canvas || !is_progress_segment_index(index)) {
        return;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(canvas);
    if (!image) {
        return;
    }
    int x0 = index * kProgressSegmentStride;
    for (int y = 0; y < kProgressSegmentH; ++y) {
        for (int x = 0; x < kProgressSegmentW; ++x) {
            bool border = is_progress_segment_border_pixel(x, y);
            lv_img_buf_set_px_color(image,
                                    x0 + x,
                                    y,
                                    (filled || border) ? lv_color_black() : lv_color_white());
        }
    }
}

void invalidate_progress_segment(lv_obj_t *canvas, int index)
{
    if (!canvas || !is_progress_segment_index(index)) {
        return;
    }
    int x0 = index * kProgressSegmentStride;
    invalidate_canvas_rect(canvas, x0, 0, x0 + kProgressSegmentW - 1, kProgressSegmentH - 1);
}
}

void build_progress_canvas(lv_obj_t *parent, lv_obj_t **canvas, lv_color_t **buf, int y)
{
    if (!parent || !canvas || !buf) {
        ESP_LOGW(TAG, "%s", UI_PROGRESS_CANVAS_BUILD_INVALID_ARG_LOG);
        return;
    }
    if (!ensure_canvas_buffer(buf, kProgressCanvasW, kProgressCanvasH)) {
        return;
    }
    *canvas = lv_canvas_create(parent);
    if (!*canvas) {
        ESP_LOGW(TAG, "%s", UI_PROGRESS_CANVAS_CREATE_FAILED_LOG);
        return;
    }
    configure_canvas_base(*canvas,
                          *buf,
                          kProgressCanvasX,
                          y,
                          kProgressCanvasW,
                          kProgressCanvasH);
    lv_canvas_fill_bg(*canvas, lv_color_white(), LV_OPA_COVER);
    for (int i = 0; i < kProgressSegmentCount; ++i) {
        draw_progress_segment(*canvas, i, false);
    }
    lv_obj_invalidate(*canvas);
}

void update_progress_canvas(lv_obj_t *canvas, int filled, int *last_filled)
{
    if (!progress_update_args_valid(canvas, last_filled)) {
        return;
    }
    filled = clamp_progress_filled_count(filled);
    if (*last_filled < 0 || filled < *last_filled) {
        for (int i = 0; i < kProgressSegmentCount; ++i) {
            draw_progress_segment(canvas, i, i < filled);
        }
        lv_obj_invalidate(canvas);
        *last_filled = filled;
        return;
    }
    if (filled == *last_filled) {
        return;
    }
    for (int i = *last_filled; i < filled; ++i) {
        draw_progress_segment(canvas, i, true);
        invalidate_progress_segment(canvas, i);
    }
    *last_filled = filled;
}

void build_work_page_day_progress(lv_obj_t *parent, int page)
{
    if (!parent ||
        !is_valid_work_page_id(page) ||
        s_work_page_day_progress_canvas[page]) {
        return;
    }
    build_progress_canvas(parent,
                          &s_work_page_day_progress_canvas[page],
                          &s_work_page_day_progress_buffer,
                          kWorkPageDayProgressY);
    if (s_work_page_day_progress_canvas[page]) {
        s_work_page_day_progress_filled = -1;
    }
}

bool update_work_page_day_progress(int page, const ClockUiTimeSnapshot &time_snapshot)
{
    if (!is_valid_work_page_id(page) ||
        !s_work_page_day_progress_canvas[page]) {
        return false;
    }
    int previous = s_work_page_day_progress_filled;
    update_progress_canvas(s_work_page_day_progress_canvas[page],
                           time_snapshot.day_progress_filled,
                           &s_work_page_day_progress_filled);
    return previous != s_work_page_day_progress_filled;
}

void set_work_page_day_progress_visible(int page, bool visible)
{
    if (!is_valid_work_page_id(page)) {
        return;
    }
    set_obj_visible(s_work_page_day_progress_canvas[page], visible);
}

void clear_work_page_day_progress_refs()
{
    for (lv_obj_t *&canvas : s_work_page_day_progress_canvas) {
        canvas = nullptr;
    }
    s_work_page_day_progress_filled = -1;
}
