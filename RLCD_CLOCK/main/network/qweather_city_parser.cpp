// 解析 QWeather 城市查询首项，不处理 HTTP、业务码或定位策略。
#include "qweather_city_parser.h"

#include "network_json.h"

#include <string.h>

namespace {
constexpr const char *kCityIdField = "id";
constexpr const char *kCityNameField = "name";
constexpr const char *kCityLatitudeField = "lat";
constexpr const char *kCityLongitudeField = "lon";
} // namespace

bool parse_qweather_city_location(const cJSON *location,
                                  char *city_id,
                                  size_t city_id_len,
                                  char *city_name,
                                  size_t city_name_len,
                                  char *lat_out,
                                  size_t lat_len,
                                  char *lon_out,
                                  size_t lon_len)
{
    if (!cJSON_IsObject(location) ||
        !city_id || city_id_len == 0 ||
        !city_name || city_name_len == 0) {
        return false;
    }
    const char *next_city_id =
        network_json_object_string_value(location, kCityIdField);
    const char *next_city_name =
        network_json_object_string_value(location, kCityNameField);
    if (!next_city_id || !next_city_name) {
        return false;
    }
    strlcpy(city_id, next_city_id, city_id_len);
    strlcpy(city_name, next_city_name, city_name_len);
    if (lat_out && lat_len > 0) {
        const char *latitude =
            network_json_object_string_value(location, kCityLatitudeField);
        if (latitude) {
            strlcpy(lat_out, latitude, lat_len);
        }
    }
    if (lon_out && lon_len > 0) {
        const char *longitude =
            network_json_object_string_value(location, kCityLongitudeField);
        if (longitude) {
            strlcpy(lon_out, longitude, lon_len);
        }
    }
    return true;
}
