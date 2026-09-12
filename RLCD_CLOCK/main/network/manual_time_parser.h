// 声明配网页手动离线时间的纯解析与校验接口。
#pragma once

#include "app_time_constants.h"

#include <time.h>

inline constexpr int kManualTimeTmYearOffset = kTmYearOffset;
inline constexpr int kManualTimeTmMonthOffset = kTmMonthOffset;

bool parse_manual_datetime_text(const char *text, struct tm *out);
