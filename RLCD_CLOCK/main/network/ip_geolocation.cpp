// 调用独立 IP 定位服务并转换为天气城市查询所需的位置文本。
#include "ip_geolocation_client.h"

#include "network_http_client.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_text_format.h"
#include "qweather_location_text.h"
#include "qweather_response.h"

#include "esp_log.h"

namespace {
constexpr size_t kIpGeoResponseBufferSize = 2048;
constexpr const char *kIpGeolocationUrl = "https://uapis.cn/api/v1/network/myip";
constexpr const char *kIpGeoStage = "ip location";
constexpr const char *kIpGeoJsonLatitudeField = "latitude";
constexpr const char *kIpGeoJsonLongitudeField = "longitude";
constexpr const char *kIpGeoJsonRegionField = "region";
constexpr const char *kIpLocationMissingCoordinateLog =
    "ip location response missing latitude/longitude";
#define IP_LOCATION_RESOLVED_FORMAT "ip location resolved: %s city=%s"

bool coordinates_available(const cJSON *latitude, const cJSON *longitude)
{
    return cJSON_IsNumber(latitude) && cJSON_IsNumber(longitude);
}

void log_ip_geolocation_warning(const char *message)
{
    ESP_LOGW(TAG, "%s", cstr_nonempty(message) ? message : kIpGeoStage);
}

static_assert(kIpGeoResponseBufferSize > 1,
              "IP geolocation response buffer must fit text and NUL");
} // namespace

bool ip_geolocation_lookup(char *location, size_t location_len, char *city, size_t city_len)
{
    if (!app_text::output_buffer_available(location, location_len) ||
        !app_text::output_buffer_available(city, city_len)) {
        log_ip_geolocation_warning(kIpLocationInvalidArgLog);
        return false;
    }
    QweatherResponseBuffer response(kIpGeoStage, kIpGeoResponseBufferSize);
    if (!response) {
        return false;
    }
    if (http_get_text(kIpGeolocationUrl, response.get(), response.size()) != ESP_OK) {
        return false;
    }
    QweatherJsonRoot root(response.get());
    if (!root) {
        return false;
    }
    const cJSON *latitude = cJSON_GetObjectItem(root.get(), kIpGeoJsonLatitudeField);
    const cJSON *longitude = cJSON_GetObjectItem(root.get(), kIpGeoJsonLongitudeField);
    const cJSON *region = cJSON_GetObjectItem(root.get(), kIpGeoJsonRegionField);
    if (!coordinates_available(latitude, longitude)) {
        log_ip_geolocation_warning(kIpLocationMissingCoordinateLog);
        return false;
    }
    if (!format_ip_coordinates(location,
                               location_len,
                               longitude->valuedouble,
                               latitude->valuedouble)) {
        return false;
    }
    const char *region_text = qweather_json_string_value(region);
    if (region_text) {
        copy_ip_region_city(city, city_len, region_text);
    }
    if (city[0] == '\0') {
        strlcpy(city, location, city_len);
    }
    ESP_LOGI(TAG, IP_LOCATION_RESOLVED_FORMAT, location, city);
    return true;
}
