// 提供所有 LVGL 对象共用的固定 PSRAM 对象池，不占用内部 DMA 内存。
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *weather_clock_lvgl_pool(size_t size);
#ifdef __cplusplus
}
#endif
