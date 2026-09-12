// 处理天气定位经纬度、地区城市和显示城市候选文本。
#include "qweather_location_text.h"

#include "app_metadata.h"
#include "app_text_format.h"
#include "weather_city_contract.h"

#include <esp_log.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr size_t kIpRegionCopySize = 96;
constexpr size_t kIpRegionMaxParts = 5;
constexpr size_t kIpRegionCityPartIndex = 2;
constexpr size_t kIpRegionCityPartMinCount = kIpRegionCityPartIndex + 1;
constexpr const char *kIpGeoCoordinateFormat = "%.4f,%.4f";
constexpr const char *kIpRegionDelimiter = " ";
constexpr const char *kIpGeoCitySuffix = "市";
bool cstr_has_suffix(const char *text, const char *suffix)
{
    if (!text || !suffix || suffix[0] == '\0') {
        return false;
    }
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    return text_len >= suffix_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

void copy_ip_city_without_suffix(char *out, size_t out_len, const char *city_part)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    strlcpy(out, city_part ? city_part : "", out_len);
    if (cstr_has_suffix(out, kIpGeoCitySuffix)) {
        out[strlen(out) - strlen(kIpGeoCitySuffix)] = '\0';
    }
}

static_assert(kIpRegionCopySize > kWeatherLocationTextSize,
              "IP region scratch buffer must be larger than displayed location text");
static_assert(kWeatherLocationTextSize <= kManualWeatherCityLen,
              "weather location text must fit manual weather city storage");
static_assert(kIpRegionCityPartIndex < kIpRegionMaxParts,
              "IP region city index must fit region parts array");
static_assert(kIpRegionCityPartMinCount <= kIpRegionMaxParts,
              "IP region city part minimum count must fit region parts array");
} // namespace

void copy_first_nonempty_text(char *out,
                              size_t out_len,
                              const char *first,
                              const char *second,
                              const char *third)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    const char *selected = first && first[0] != '\0'
                               ? first
                               : (second && second[0] != '\0' ? second : (third && third[0] != '\0' ? third : ""));
    strlcpy(out, selected, out_len);
}

bool format_ip_coordinates(char *out, size_t out_len, double longitude, double latitude)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        ESP_LOGW(TAG, "%s", kIpLocationInvalidArgLog);
        return false;
    }
    int written = snprintf(out, out_len, kIpGeoCoordinateFormat, longitude, latitude);
    if (app_text::format_failed(written, out_len)) {
        out[0] = '\0';
        ESP_LOGW(TAG, "%s", kIpLocationCoordinateTooLongLog);
        return false;
    }
    return true;
}

void copy_ip_coordinate_location(const char *location,
                                 char *city_id,
                                 size_t city_id_len,
                                 WeatherData *weather)
{
    if (!location || !app_text::output_buffer_available(city_id, city_id_len) || !weather) {
        return;
    }
    strlcpy(city_id, location, city_id_len);
    char *comma = strchr(city_id, ',');
    if (!comma) {
        return;
    }
    size_t lon_len = comma - city_id;
    if (lon_len >= sizeof(weather->lon)) {
        lon_len = sizeof(weather->lon) - 1;
    }
    memcpy(weather->lon, city_id, lon_len);
    weather->lon[lon_len] = '\0';
    strlcpy(weather->lat, comma + 1, sizeof(weather->lat));
}

void copy_ip_region_city(char *out, size_t out_len, const char *region)
{
    if (!app_text::output_buffer_available(out, out_len) || !region || region[0] == '\0') {
        return;
    }

    char region_copy[kIpRegionCopySize] = {};
    strlcpy(region_copy, region, sizeof(region_copy));
    char *parts[kIpRegionMaxParts] = {};
    size_t count = 0;
    char *save = nullptr;
    char *token = strtok_r(region_copy, kIpRegionDelimiter, &save);
    while (token && count < kIpRegionMaxParts) {
        if (token[0] != '\0') {
            parts[count++] = token;
        }
        token = strtok_r(nullptr, kIpRegionDelimiter, &save);
    }

    const char *city_part = count >= kIpRegionCityPartMinCount
                                ? parts[kIpRegionCityPartIndex]
                                : (count > 0 ? parts[count - 1] : "");
    copy_ip_city_without_suffix(out, out_len, city_part);
}
