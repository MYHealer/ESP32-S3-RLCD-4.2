// 定义温湿时钟构建与运行刷新之间的页面对象引用契约。
#pragma once

#include <lvgl.h>

inline constexpr int kFlipClockObjectCardCount = 3;

struct FlipClockObjectRefs {
    lv_obj_t *card_canvas[kFlipClockObjectCardCount] = {};
    lv_obj_t *sensor_label = nullptr;
    lv_obj_t *sensor_bold_label = nullptr;
    lv_obj_t *sensor_bold_y_label = nullptr;
    lv_obj_t *humidity_label = nullptr;
    lv_obj_t *humidity_bold_label = nullptr;
    lv_obj_t *humidity_bold_y_label = nullptr;
    lv_obj_t *temp_mood_canvas = nullptr;
    lv_obj_t *humi_mood_canvas = nullptr;
    lv_obj_t *temp_trend_canvas = nullptr;
    lv_obj_t *humi_trend_canvas = nullptr;
    lv_obj_t *day_label = nullptr;
    lv_obj_t *day_bold_label = nullptr;
    lv_obj_t *day_bold_y_label = nullptr;
    lv_obj_t *lunar_label = nullptr;
    lv_obj_t *lunar_bold_x_label = nullptr;
    lv_obj_t *lunar_bold_y_label = nullptr;
    lv_obj_t *lunar_bold_xy_label = nullptr;
};

FlipClockObjectRefs &mutable_flip_clock_object_refs();
const FlipClockObjectRefs &flip_clock_object_refs();
