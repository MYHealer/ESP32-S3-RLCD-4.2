// 构建 SDL 温湿历史页面的曲线、坐标轴和极值标记主体。
#pragma once

#include <time.h>

#include "lvgl.h"

void build_history_preview_body(lv_obj_t *screen, struct tm *local);
