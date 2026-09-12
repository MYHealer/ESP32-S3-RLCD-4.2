// 实现 SDL 工作页共享进度条的创建、逐段绘制和最小区域失效。
#include "sdl_preview_progress.h"

#include "sdl_preview_progress_math.h"

namespace sdl_preview_progress {
namespace {

constexpr int kSegmentWidth = 5;
constexpr int kSegmentHeight = 3;
constexpr int kSegmentGap = 1;
constexpr int kCanvasWidth =
    kSegmentCount * kSegmentWidth + (kSegmentCount - 1) * kSegmentGap;
constexpr int kCanvasHeight = kSegmentHeight;

void draw_segment(lv_obj_t *canvas, int index, bool filled)
{
    if (!canvas || index < 0 || index >= kSegmentCount) {
        return;
    }
    const int x0 = index * (kSegmentWidth + kSegmentGap);
    for (int y = 0; y < kSegmentHeight; ++y) {
        for (int x = 0; x < kSegmentWidth; ++x) {
            const bool border = x == 0 || x == kSegmentWidth - 1 ||
                                y == 0 || y == kSegmentHeight - 1;
            lv_canvas_set_px_color(
                canvas,
                x0 + x,
                y,
                (filled || border) ? lv_color_black() : lv_color_white());
        }
    }
}

void invalidate_segment(lv_obj_t *canvas, int index)
{
    if (!canvas || index < 0 || index >= kSegmentCount) {
        return;
    }
    const int x0 = index * (kSegmentWidth + kSegmentGap);
    lv_area_t area = {};
    area.x1 = static_cast<lv_coord_t>(x0);
    area.y1 = 0;
    area.x2 = static_cast<lv_coord_t>(x0 + kSegmentWidth - 1);
    area.y2 = static_cast<lv_coord_t>(kSegmentHeight - 1);
    lv_obj_invalidate_area(canvas, &area);
}

}  // namespace

Canvas::Canvas() : pixels_(kCanvasWidth * kCanvasHeight)
{
}

void Canvas::build(lv_obj_t *parent, int y)
{
    canvas_ = lv_canvas_create(parent);
    last_filled_ = -1;
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(canvas_, 20, y);
    lv_obj_set_size(canvas_, kCanvasWidth, kCanvasHeight);
    lv_obj_set_style_border_width(canvas_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(canvas_, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(
        canvas_, pixels_.data(), kCanvasWidth, kCanvasHeight, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(canvas_, lv_color_white(), LV_OPA_COVER);
    for (int index = 0; index < kSegmentCount; ++index) {
        draw_segment(canvas_, index, false);
    }
    lv_obj_invalidate(canvas_);
}

void Canvas::build_day(lv_obj_t *parent, const struct tm &local, int y)
{
    build(parent, y);
    update(filled_segments_for_day(local.tm_hour, local.tm_min, local.tm_sec));
}

void Canvas::update(int filled)
{
    if (!canvas_) {
        return;
    }
    filled = clamp_filled_segments(filled);
    if (last_filled_ < 0 || filled < last_filled_) {
        for (int index = 0; index < kSegmentCount; ++index) {
            draw_segment(canvas_, index, index < filled);
        }
        lv_obj_invalidate(canvas_);
        last_filled_ = filled;
        return;
    }
    if (filled == last_filled_) {
        return;
    }
    for (int index = last_filled_; index < filled; ++index) {
        draw_segment(canvas_, index, true);
        invalidate_segment(canvas_, index);
    }
    last_filled_ = filled;
}

lv_obj_t *Canvas::object() const
{
    return canvas_;
}

}  // namespace sdl_preview_progress
