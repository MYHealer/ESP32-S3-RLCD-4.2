// 实现温湿历史窗口整理，不包含 NVS 读取、LVGL 绘制或页面状态。
#include "ui_history_window.h"

static_assert(kHistoryWindowHours > 0, "history window must be positive");
static_assert(kHistorySecondsPerHour > 0, "history seconds per hour must be positive");
static_assert(kHistoryWindowHours <= kHourlyHistoryCount,
              "history display window must fit persisted sample storage");

bool collect_history_window_from_snapshot(time_t end_hour,
                                          const HourlySensorHistoryBlob &history,
                                          HourlySensorSample *out,
                                          int *out_count)
{
    if (out_count) {
        *out_count = 0;
    }
    if (!out || !out_count) {
        return false;
    }
    time_t start = end_hour - kHistoryWindowHours * kHistorySecondsPerHour;
    int count = 0;
    int slot_count = history.count <= kHourlyHistoryCount ? history.count : kHourlyHistoryCount;
    for (int hour = 1; hour <= kHistoryWindowHours; ++hour) {
        time_t expected = start + hour * kHistorySecondsPerHour;
        bool found = false;
        for (int i = 0; i < slot_count; ++i) {
            const HourlySensorSample &sample = history.samples[i];
            if (sample.valid && sample.timestamp == expected) {
                out[count++] = sample;
                found = true;
                break;
            }
        }
        if (!found) {
            HourlySensorSample sample = {};
            sample.timestamp = expected;
            out[count++] = sample;
        }
    }
    *out_count = count;
    return count > 0;
}
