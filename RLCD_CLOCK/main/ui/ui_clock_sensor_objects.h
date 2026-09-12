// 定义天气时钟本地传感器面板构建与刷新之间的对象引用契约。
#pragma once

#include <lvgl.h>

struct ClockLocalSensorObjectRefs {
    lv_obj_t *temperature_icon_canvas = nullptr;
    lv_obj_t *humidity_icon_canvas = nullptr;
    lv_obj_t *temperature_label = nullptr;
    lv_obj_t *humidity_label = nullptr;
    lv_obj_t *temperature_trend_canvas = nullptr;
    lv_obj_t *humidity_trend_canvas = nullptr;
};

ClockLocalSensorObjectRefs &mutable_clock_local_sensor_object_refs();
const ClockLocalSensorObjectRefs &clock_local_sensor_object_refs();
