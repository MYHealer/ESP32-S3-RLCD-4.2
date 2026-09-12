// 声明 SDL 温湿时钟页面主体预览构建接口。
#pragma once

#include <ctime>

#include "lvgl.h"

void build_flip_clock_preview_body(lv_obj_t *screen, const struct tm *local);
