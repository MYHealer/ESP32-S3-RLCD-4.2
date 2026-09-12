// 验证 IP 定位候选、经纬度格式和城市后缀处理。
#include "qweather_location_text.h"

#include <assert.h>
#include <string.h>

int main()
{
    char text[64] = {};
    copy_first_nonempty_text(text, sizeof(text), "", "杭州", "上海");
    assert(strcmp(text, "杭州") == 0);
    copy_first_nonempty_text(text, sizeof(text), nullptr, "", "上海");
    assert(strcmp(text, "上海") == 0);
    copy_first_nonempty_text(text, sizeof(text), nullptr, nullptr, nullptr);
    assert(text[0] == '\0');

    assert(format_ip_coordinates(text, sizeof(text), 120.125, 30.5));
    assert(strcmp(text, "120.1250,30.5000") == 0);
    char too_small[4] = {'x', 'x', 'x', '\0'};
    assert(!format_ip_coordinates(too_small,
                                  sizeof(too_small),
                                  120.125,
                                  30.5));
    assert(too_small[0] == '\0');
    assert(!format_ip_coordinates(nullptr, 0, 120.0, 30.0));

    WeatherData weather = {};
    char city_id[32] = {};
    copy_ip_coordinate_location("120.1235,30.9877",
                                city_id,
                                sizeof(city_id),
                                &weather);
    assert(strcmp(city_id, "120.1235,30.9877") == 0);
    assert(strcmp(weather.lon, "120.1235") == 0);
    assert(strcmp(weather.lat, "30.9877") == 0);

    copy_ip_region_city(text, sizeof(text), "中国 浙江 杭州市 联通");
    assert(strcmp(text, "杭州") == 0);
    copy_ip_region_city(text, sizeof(text), "上海市");
    assert(strcmp(text, "上海") == 0);
    strcpy(text, "保留");
    copy_ip_region_city(text, sizeof(text), nullptr);
    assert(strcmp(text, "保留") == 0);
    return 0;
}
