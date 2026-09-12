// 定义天气时钟顶部状态栏构建与刷新之间的对象引用契约。
#pragma once

#include <lvgl.h>

struct ClockHeaderObjectRefs {
    lv_obj_t *date_label = nullptr;
    lv_obj_t *alert_pill = nullptr;
    lv_obj_t *alert_icon_canvas = nullptr;
    lv_obj_t *alert_label = nullptr;
    lv_obj_t *chime_status_icon_canvas = nullptr;
    lv_obj_t *wifi_status_icon_canvas = nullptr;
    lv_obj_t *alarm_status_icon_canvas = nullptr;
};

ClockHeaderObjectRefs &mutable_clock_header_object_refs();
const ClockHeaderObjectRefs &clock_header_object_refs();
