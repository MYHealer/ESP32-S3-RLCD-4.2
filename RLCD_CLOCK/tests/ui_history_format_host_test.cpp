// 验证温湿历史页坐标范围、像素映射和显示文本保持稳定。
#include "ui_history_format.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr float kFloatTolerance = 0.001f;

bool near(float actual, float expected)
{
    return fabsf(actual - expected) < kFloatTolerance;
}
} // namespace

int main()
{
    setenv("TZ", "Asia/Shanghai", 1);
    tzset();

    HistoryAxisRange temperature = history_axis_range(true, 20.0f, 20.5f);
    assert(near(temperature.minimum, 18.9f));
    assert(near(temperature.midpoint, 20.25f));
    assert(near(temperature.maximum, 21.6f));

    HistoryAxisRange humidity = history_axis_range(false, 50.0f, 52.0f);
    assert(near(humidity.minimum, 45.0f));
    assert(near(humidity.midpoint, 51.0f));
    assert(near(humidity.maximum, 57.0f));

    assert(history_value_to_plot_y(0.0f, 0.0f, 10.0f, 10, 62) == 71);
    assert(history_value_to_plot_y(5.0f, 0.0f, 10.0f, 10, 62) == 40);
    assert(history_value_to_plot_y(10.0f, 0.0f, 10.0f, 10, 62) == 10);
    assert(history_value_to_plot_y(8.0f, 8.0f, 8.0f, 12, 20) == 31);
    assert(history_value_to_plot_y(5.0f, 0.0f, 10.0f, 14, 0) == 14);

    struct tm local = {};
    local.tm_year = 126;
    local.tm_mon = 6;
    local.tm_mday = 12;
    local.tm_hour = 7;
    time_t value = mktime(&local);
    char text[16] = {};
    format_history_axis_hour(value, text, sizeof(text));
    assert(strcmp(text, "07:00") == 0);

    format_history_axis_value(true, -3.6f, text, sizeof(text));
    assert(strcmp(text, "-4℃") == 0);
    format_history_axis_value(false, 58.4f, text, sizeof(text));
    assert(strcmp(text, "58%") == 0);
    format_history_badge_value(true, 25.64f, text, sizeof(text));
    assert(strcmp(text, "25.6") == 0);
    format_history_badge_value(false, 58.4f, text, sizeof(text));
    assert(strcmp(text, "58") == 0);

    char short_text[3] = {};
    format_history_axis_value(true, 25.0f, short_text, sizeof(short_text));
    assert(strcmp(short_text, "--") == 0);
    format_history_badge_value(false, 50.0f, nullptr, 0);
    return 0;
}
