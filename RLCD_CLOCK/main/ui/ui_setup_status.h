// 声明天气时钟配网模式状态面板的构建和刷新接口。
#pragma once

#include "lvgl.h"

void build_setup_status_panel(lv_obj_t *parent);
bool update_setup_status_panel();
void set_setup_status_panel_visible(bool visible);
void clear_setup_status_object_refs();
