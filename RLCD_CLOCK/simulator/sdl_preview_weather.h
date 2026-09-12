// 提供 SDL 天气看板主体和预览共用的 QWeather 图标文本转换。
#pragma once

#include "lvgl.h"

const char *preview_weather_icon_text(const char *code);
void build_weather_board_preview_body(lv_obj_t *screen);
