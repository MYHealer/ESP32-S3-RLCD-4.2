// 验证 QWeather 预警标题组装、颜色等级、压缩和排序规则。
#include "qweather_alert_text.h"

#include <assert.h>
#include <string.h>

int main()
{
    assert(strcmp(warning_color_name("yellow"), "黄色") == 0);
    assert(warning_color_rank("red") > warning_color_rank("orange"));
    assert(warning_color_rank("unknown") == 0);

    char title[kWeatherAlertTitleLen] = {};
    assert(build_weather_alert_title(title, sizeof(title), "大风", "yellow", "备用标题"));
    assert(strcmp(title, "大风黄色预警") == 0);

    assert(build_weather_alert_title(title, sizeof(title), "", "", "雷暴预警标题"));
    assert(strcmp(title, "雷暴预警标题") == 0);

    assert(build_weather_alert_title(title, sizeof(title), "大风", "unknown", ""));
    assert(strcmp(title, "大风预警") == 0);

    assert(build_weather_alert_title(title, sizeof(title), "", "", ""));
    assert(title[0] == '\0');
    assert(!build_weather_alert_title(nullptr, 0, "大风", "yellow", ""));

    WeatherAlertData alerts = {};
    add_weather_alert_title(&alerts, "低温蓝色预警", warning_color_rank("blue"));
    add_weather_alert_title(&alerts, "暴雨红色预警", warning_color_rank("red"));
    add_weather_alert_title(&alerts, "超强大风橙色预警", warning_color_rank("orange"));
    assert(alerts.active);
    assert(alerts.count == 3);
    assert(strcmp(alerts.titles[0], "暴雨红色预警") == 0);
    assert(strcmp(alerts.titles[1], "超强大风橙") == 0);
    assert(strcmp(alerts.titles[2], "低温蓝色预警") == 0);
    assert(alerts.ranks[0] > alerts.ranks[1]);
    assert(alerts.ranks[1] > alerts.ranks[2]);

    return 0;
}
