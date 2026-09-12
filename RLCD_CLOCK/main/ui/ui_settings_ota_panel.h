// 声明设置页 OTA 状态面板的构建和刷新接口。
#pragma once

#include "lvgl.h"

struct OtaRuntimeSnapshot;

void build_settings_ota_panel(lv_obj_t *screen, int panel_x, int panel_width);
bool update_settings_ota_panel(bool visible, const OtaRuntimeSnapshot &ota);
void clear_settings_ota_panel_object_refs();
