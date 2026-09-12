// 声明 LVGL 到 RLCD 的底层显示刷新回调。
#pragma once

#include "lvgl.h"

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
