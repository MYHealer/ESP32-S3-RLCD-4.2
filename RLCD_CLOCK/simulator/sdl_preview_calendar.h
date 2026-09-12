// 构建 SDL 日历页面的星期栏、日期格和农历预览主体。
#pragma once

#include <time.h>

#include "lvgl.h"

void build_calendar_preview_body(lv_obj_t *screen, const struct tm *local);
