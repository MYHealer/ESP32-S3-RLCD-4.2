// 声明小智工作页的构建、刷新、动画调度和对象清理接口。
#pragma once

#include <stdint.h>
#include <time.h>

void build_xiaozhi_page();
bool update_xiaozhi_page(const struct tm &local);
uint32_t xiaozhi_subtitle_animation_delay_ms();
void clear_xiaozhi_page_object_refs();
