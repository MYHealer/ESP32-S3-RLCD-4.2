// 声明聚合时钟页面的构建、可见刷新及对象清理入口。
#pragma once
#include <time.h>
void build_aggregate_clock_page();
bool update_aggregate_clock_page(const struct tm &local);
void clear_aggregate_clock_refs();
