// 声明温湿时钟的构建、刷新和页面对象清理接口。
#pragma once

#include <time.h>

struct ClockUiTimeSnapshot;

void build_flip_clock_page();
bool update_flip_clock_page(const struct tm &local,
                            const ClockUiTimeSnapshot &time_snapshot);
bool update_flip_clock_sensor_status();
void clear_flip_clock_object_refs();
