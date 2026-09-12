// 定义天气时钟主体构建与运行期刷新之间的对象引用契约。
#pragma once

#include <lvgl.h>

struct ClockSurfaceObjectRefs {
    lv_obj_t *low_battery_icon_canvas = nullptr;
    lv_obj_t *panel_separator_a = nullptr;
    lv_obj_t *panel_separator_b = nullptr;
    lv_obj_t *time_canvas = nullptr;
    lv_obj_t *second_canvas = nullptr;
    lv_obj_t *status_gif_canvas = nullptr;
    lv_obj_t *second_progress_canvas = nullptr;
};

ClockSurfaceObjectRefs &mutable_clock_surface_object_refs();
const ClockSurfaceObjectRefs &clock_surface_object_refs();
