// 验证小智单色表情的空对象保护、帧缓存、情绪变化和动画节拍。
#include "ui_xiaozhi_face.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace {
uint32_t s_tick = 0;
int s_fill_count = 0;
int s_line_count = 0;
int s_circle_count = 0;
int s_invalidate_count = 0;

void reset_draw_counts()
{
    s_fill_count = 0;
    s_line_count = 0;
    s_circle_count = 0;
    s_invalidate_count = 0;
}

XiaozhiAiSnapshot snapshot(XiaozhiAiState state, const char *emotion = "")
{
    XiaozhiAiSnapshot value = {};
    value.state = state;
    snprintf(value.emotion, sizeof(value.emotion), "%s", emotion ? emotion : "");
    return value;
}
} // namespace

uint32_t lv_tick_get()
{
    return s_tick;
}

lv_color_t lv_color_black()
{
    return 0;
}

lv_color_t lv_color_white()
{
    return 1;
}

void lv_canvas_fill_bg(lv_obj_t *, lv_color_t, int)
{
    ++s_fill_count;
}

void lv_obj_invalidate(lv_obj_t *)
{
    ++s_invalidate_count;
}

void canvas_draw_line(lv_obj_t *, int, int, int, int, int, int, lv_color_t)
{
    ++s_line_count;
}

void canvas_draw_filled_circle(lv_obj_t *, int, int, int, int, int, lv_color_t)
{
    ++s_circle_count;
}

int main()
{
    lv_obj_t first_canvas = {1};
    lv_obj_t second_canvas = {2};

    reset_draw_counts();
    assert(!update_xiaozhi_face(nullptr, snapshot(kXiaozhiAiReady)));
    assert(s_fill_count == 0 && s_invalidate_count == 0);

    s_tick = 0;
    assert(update_xiaozhi_face(&first_canvas, snapshot(kXiaozhiAiReady)));
    assert(s_fill_count == 1);
    assert(s_circle_count == 4);
    assert(s_line_count == 1);
    assert(s_invalidate_count == 1);

    reset_draw_counts();
    s_tick = 100;
    assert(!update_xiaozhi_face(&first_canvas, snapshot(kXiaozhiAiReady)));
    assert(s_fill_count == 0 && s_invalidate_count == 0);

    assert(update_xiaozhi_face(&first_canvas,
                               snapshot(kXiaozhiAiReady, "happy")));
    assert(s_fill_count == 1);
    assert(s_line_count == 8);
    assert(s_invalidate_count == 1);

    reset_draw_counts();
    s_tick = 0;
    assert(update_xiaozhi_face(&first_canvas,
                               snapshot(kXiaozhiAiListening)));
    assert(s_invalidate_count == 1);
    reset_draw_counts();
    s_tick = kXiaozhiFaceFrameIntervalMs - 1;
    assert(!update_xiaozhi_face(&first_canvas,
                                snapshot(kXiaozhiAiListening)));
    assert(s_invalidate_count == 0);
    s_tick = kXiaozhiFaceFrameIntervalMs;
    assert(update_xiaozhi_face(&first_canvas,
                               snapshot(kXiaozhiAiListening)));
    assert(s_invalidate_count == 1);

    reset_draw_counts();
    assert(update_xiaozhi_face(&second_canvas,
                               snapshot(kXiaozhiAiListening)));
    assert(s_fill_count == 1 && s_invalidate_count == 1);

    return 0;
}
