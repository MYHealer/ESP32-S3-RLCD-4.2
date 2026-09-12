// 声明 UI 使用的完整日期时间安全格式化接口。
#pragma once

#include <stddef.h>
#include <time.h>

void format_time_or_dash(time_t value, char *out, size_t out_len);
