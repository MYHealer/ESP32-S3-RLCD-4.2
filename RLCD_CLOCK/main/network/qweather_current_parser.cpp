// 解析 QWeather 实时天气和空气质量字段，不处理 HTTP 或缓存发布时间。
#include "qweather_current_parser.h"

#include "network_json.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr const char *kWeatherTextField = "text";
constexpr const char *kWeatherIconField = "icon";
constexpr const char *kWeatherTempField = "temp";
constexpr const char *kWeatherHumidityField = "humidity";
constexpr const char *kAirIndexesField = "indexes";
constexpr const char *kAirPollutantsField = "pollutants";
constexpr const char *kAirCodeField = "code";
constexpr const char *kAirAqiField = "aqi";
constexpr const char *kAirAqiDisplayField = "aqiDisplay";
constexpr const char *kAirCategoryField = "category";
constexpr const char *kAirPrimaryPollutantField = "primaryPollutant";
constexpr const char *kAirConcentrationField = "concentration";
constexpr const char *kAirConcentrationValueField = "value";
constexpr const char *kAirPm25Code = "pm2p5";

bool format_json_number(const cJSON *item, char *out, size_t out_len)
{
    if (!cJSON_IsNumber(item) || !out || out_len == 0) {
        return false;
    }
    int written = snprintf(out, out_len, "%.0f", item->valuedouble);
    return written >= 0 && static_cast<size_t>(written) < out_len;
}

const cJSON *find_pollutant(const cJSON *pollutants, const char *code)
{
    if (!cJSON_IsArray(pollutants) || !code) {
        return nullptr;
    }
    const int count = cJSON_GetArraySize(pollutants);
    for (int i = 0; i < count; ++i) {
        const cJSON *item = cJSON_GetArrayItem(pollutants, i);
        const char *item_code =
            network_json_object_string_value(item, kAirCodeField);
        if (item_code && strcmp(item_code, code) == 0) {
            return item;
        }
    }
    return nullptr;
}

} // namespace

bool parse_qweather_current_weather(const cJSON *now, WeatherData *weather)
{
    if (!cJSON_IsObject(now) || !weather) {
        return false;
    }
    const char *text = network_json_object_string_value(now, kWeatherTextField);
    const char *icon = network_json_object_string_value(now, kWeatherIconField);
    const char *temp = network_json_object_string_value(now, kWeatherTempField);
    const char *humidity =
        network_json_object_string_value(now, kWeatherHumidityField);
    if (!text || !icon || !temp || !humidity) {
        return false;
    }
    strlcpy(weather->text, text, sizeof(weather->text));
    strlcpy(weather->icon, icon, sizeof(weather->icon));
    strlcpy(weather->temp, temp, sizeof(weather->temp));
    strlcpy(weather->humidity, humidity, sizeof(weather->humidity));
    return true;
}

bool parse_qweather_current_air(const cJSON *root, WeatherAirData *air)
{
    if (!cJSON_IsObject(root) || !air) {
        return false;
    }
    const cJSON *indexes = cJSON_GetObjectItem(root, kAirIndexesField);
    const cJSON *index =
        cJSON_IsArray(indexes) ? cJSON_GetArrayItem(indexes, 0) : nullptr;
    if (!cJSON_IsObject(index)) {
        return false;
    }
    char aqi_text[sizeof(WeatherAirData{}.aqi)] = {};
    const char *aqi_display =
        network_json_object_string_value(index, kAirAqiDisplayField);
    if (aqi_display) {
        strlcpy(aqi_text, aqi_display, sizeof(aqi_text));
    } else if (!format_json_number(cJSON_GetObjectItem(index, kAirAqiField),
                                   aqi_text,
                                   sizeof(aqi_text))) {
        return false;
    }
    const char *category =
        network_json_object_string_value(index, kAirCategoryField);
    if (aqi_text[0] == '\0' || !category) {
        return false;
    }
    WeatherAirData next = *air;
    strlcpy(next.aqi, aqi_text, sizeof(next.aqi));
    strlcpy(next.category, category, sizeof(next.category));
    next.primary[0] = '\0';
    next.pm2p5[0] = '\0';
    const cJSON *primary =
        cJSON_GetObjectItem(index, kAirPrimaryPollutantField);
    const char *primary_code =
        network_json_object_string_value(primary, kAirCodeField);
    if (primary_code) {
        strlcpy(next.primary,
                strcmp(primary_code, "na") == 0 ? "NA" : primary_code,
                sizeof(next.primary));
    }
    const cJSON *pm25 =
        find_pollutant(cJSON_GetObjectItem(root, kAirPollutantsField),
                       kAirPm25Code);
    const cJSON *concentration =
        cJSON_GetObjectItem(pm25, kAirConcentrationField);
    (void)format_json_number(
        cJSON_GetObjectItem(concentration, kAirConcentrationValueField),
        next.pm2p5,
        sizeof(next.pm2p5));
    *air = next;
    return true;
}
