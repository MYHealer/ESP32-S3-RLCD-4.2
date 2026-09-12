// 声明天气时钟 DSEG 数字 canvas 刷新接口。
#pragma once

#include <time.h>

void draw_time_canvas(const struct tm &local);
void draw_second_canvas(const struct tm &local);
