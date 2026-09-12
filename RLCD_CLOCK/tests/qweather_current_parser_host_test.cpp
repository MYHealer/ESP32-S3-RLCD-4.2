// 验证 QWeather 实时天气必填字段和空气质量必填/可选字段解析语义。
#include "qweather_current_parser.h"

#include "cJSON.h"

#include <assert.h>
#include <string.h>

namespace {
cJSON *parse_json(const char *text)
{
    cJSON *root = cJSON_Parse(text);
    assert(root != nullptr);
    return root;
}

void assert_weather_equal(const WeatherData &actual,
                          const WeatherData &expected)
{
    assert(strcmp(actual.city, expected.city) == 0);
    assert(strcmp(actual.text, expected.text) == 0);
    assert(strcmp(actual.icon, expected.icon) == 0);
    assert(strcmp(actual.temp, expected.temp) == 0);
    assert(strcmp(actual.humidity, expected.humidity) == 0);
    assert(strcmp(actual.lat, expected.lat) == 0);
    assert(strcmp(actual.lon, expected.lon) == 0);
}

void assert_air_equal(const WeatherAirData &actual,
                      const WeatherAirData &expected)
{
    assert(actual.ready == expected.ready);
    assert(strcmp(actual.aqi, expected.aqi) == 0);
    assert(strcmp(actual.category, expected.category) == 0);
    assert(strcmp(actual.primary, expected.primary) == 0);
    assert(strcmp(actual.pm2p5, expected.pm2p5) == 0);
    assert(actual.updated_at == expected.updated_at);
}
} // namespace

int main()
{
    WeatherData weather = {};
    assert(!parse_qweather_current_weather(nullptr, &weather));
    cJSON *array = parse_json("[]");
    assert(!parse_qweather_current_weather(array, &weather));
    cJSON_Delete(array);

    cJSON *now = parse_json(
        "{\"text\":\"多云\",\"icon\":\"101\",\"temp\":\"26\",\"humidity\":\"58\"}");
    strlcpy(weather.city, "杭州", sizeof(weather.city));
    strlcpy(weather.lat, "30.28", sizeof(weather.lat));
    strlcpy(weather.lon, "120.15", sizeof(weather.lon));
    assert(parse_qweather_current_weather(now, &weather));
    assert(strcmp(weather.text, "多云") == 0);
    assert(strcmp(weather.icon, "101") == 0);
    assert(strcmp(weather.temp, "26") == 0);
    assert(strcmp(weather.humidity, "58") == 0);
    assert(strcmp(weather.city, "杭州") == 0);
    assert(strcmp(weather.lat, "30.28") == 0);
    assert(strcmp(weather.lon, "120.15") == 0);
    cJSON_Delete(now);

    WeatherData partial_weather = weather;
    now = parse_json("{\"text\":\"晴\",\"icon\":\"100\",\"temp\":\"31\"}");
    assert(!parse_qweather_current_weather(now, &partial_weather));
    assert_weather_equal(partial_weather, weather);
    cJSON_Delete(now);

    WeatherData wrong_type_weather = weather;
    now = parse_json(
        "{\"text\":\"晴\",\"icon\":100,\"temp\":\"31\",\"humidity\":\"52\"}");
    assert(!parse_qweather_current_weather(now, &wrong_type_weather));
    assert_weather_equal(wrong_type_weather, weather);
    cJSON_Delete(now);

    WeatherAirData air = {};
    air.ready = true;
    air.updated_at = 123456;
    now = parse_json(
        "{\"indexes\":[{\"code\":\"cn-mee\",\"aqi\":42,\"aqiDisplay\":\"42\","
        "\"category\":\"优\",\"primaryPollutant\":{\"code\":\"na\"}}],"
        "\"pollutants\":[{\"code\":\"pm2p5\",\"concentration\":{\"value\":17,"
        "\"unit\":\"μg/m3\"}}]}");
    assert(parse_qweather_current_air(now, &air));
    assert(strcmp(air.aqi, "42") == 0);
    assert(strcmp(air.category, "优") == 0);
    assert(strcmp(air.primary, "NA") == 0);
    assert(strcmp(air.pm2p5, "17") == 0);
    assert(air.ready && air.updated_at == 123456);
    cJSON_Delete(now);

    WeatherAirData air_without_optional = {};
    strlcpy(air_without_optional.primary, "stale-primary",
            sizeof(air_without_optional.primary));
    strlcpy(air_without_optional.pm2p5, "stale-pm2p5",
            sizeof(air_without_optional.pm2p5));
    now = parse_json(
        "{\"indexes\":[{\"code\":\"cn-mee\",\"aqi\":80,\"category\":\"良\"}],"
        "\"pollutants\":[]}");
    assert(parse_qweather_current_air(now, &air_without_optional));
    assert(air_without_optional.primary[0] == '\0');
    assert(air_without_optional.pm2p5[0] == '\0');
    cJSON_Delete(now);

    WeatherAirData partial_air = air;
    now = parse_json(
        "{\"indexes\":[{\"code\":\"cn-mee\",\"aqi\":90}],\"pollutants\":[]}");
    assert(!parse_qweather_current_air(now, &partial_air));
    assert_air_equal(partial_air, air);
    cJSON_Delete(now);

    WeatherAirData short_circuit_air = air;
    now = parse_json(
        "{\"indexes\":[{\"code\":\"cn-mee\",\"category\":\"优\"}],"
        "\"pollutants\":[]}");
    assert(!parse_qweather_current_air(now, &short_circuit_air));
    assert_air_equal(short_circuit_air, air);
    cJSON_Delete(now);

    WeatherAirData wrong_type_air = air;
    now = parse_json(
        "{\"indexes\":\"bad\",\"pollutants\":[]}");
    assert(!parse_qweather_current_air(now, &wrong_type_air));
    assert_air_equal(wrong_type_air, air);
    cJSON_Delete(now);
    return 0;
}
