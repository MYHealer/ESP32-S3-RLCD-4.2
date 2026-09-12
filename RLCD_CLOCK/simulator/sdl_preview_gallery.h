// 构建 SDL 图片时钟的内置图片、块状时间和每日文字预览主体。
#pragma once

#include <time.h>

#include "lvgl.h"

void build_gallery_preview_body(lv_obj_t *screen, const struct tm *local);
