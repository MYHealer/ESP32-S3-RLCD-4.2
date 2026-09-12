// 定义天气时钟天气面板构建与文本刷新之间的对象引用契约。
#pragma once

#include <lvgl.h>

struct ClockWeatherPanelObjectRefs {
    lv_obj_t *city_label = nullptr;
    lv_obj_t *info_label = nullptr;
    lv_obj_t *icon_label = nullptr;
    lv_obj_t *temperature_label = nullptr;
    lv_obj_t *humidity_label = nullptr;
};

ClockWeatherPanelObjectRefs &mutable_clock_weather_panel_object_refs();
