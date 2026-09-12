// 声明工作页共用的电池图标构建、刷新和对象引用清理接口。
#pragma once

#include "lvgl.h"

void build_work_page_battery_icon(lv_obj_t *parent, int page);
void update_work_page_battery_icon(int page,
                                   int percent,
                                   bool charging = false,
                                   bool blink_on = true);
void clear_work_page_battery_refs();
