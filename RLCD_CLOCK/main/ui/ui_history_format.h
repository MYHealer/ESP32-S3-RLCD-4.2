// 提供温湿历史页坐标轴范围、曲线坐标和文本格式的纯计算接口。
#pragma once

#include <stddef.h>
#include <time.h>

struct HistoryAxisRange {
    float minimum;
    float midpoint;
    float maximum;
};

HistoryAxisRange history_axis_range(bool temperature, float minimum, float maximum);
int history_value_to_plot_y(float value, float minimum, float maximum, int y, int height);
void format_history_axis_hour(time_t value, char *out, size_t out_len);
void format_history_axis_value(bool temperature, float value, char *out, size_t out_len);
void format_history_badge_value(bool temperature, float value, char *out, size_t out_len);
