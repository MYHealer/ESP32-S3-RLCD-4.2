// 提供温湿历史页最近 24 个整点样本窗口的纯整理逻辑。
#pragma once

#include "sensor_history_types.h"

#include <time.h>

inline constexpr int kHistoryWindowHours = 24;
inline constexpr int kHistorySecondsPerHour = 60 * 60;

bool collect_history_window_from_snapshot(time_t end_hour,
                                          const HourlySensorHistoryBlob &history,
                                          HourlySensorSample *out,
                                          int *out_count);
