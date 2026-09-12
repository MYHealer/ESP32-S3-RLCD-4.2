// 为小智编解码运行时主机测试声明可控堆分配入口。
#pragma once

#include <stddef.h>

#define MALLOC_CAP_SPIRAM 0x01
#define MALLOC_CAP_8BIT 0x02

#ifdef __cplusplus
extern "C" {
#endif

void *heap_caps_calloc(size_t count, size_t size, unsigned caps);

#ifdef __cplusplus
}
#endif
