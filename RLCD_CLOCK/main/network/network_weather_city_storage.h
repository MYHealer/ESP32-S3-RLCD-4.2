// 声明手动天气城市与自定义资源城市之间的 NVS 存储和优先级规则。
#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "nvs.h"

namespace network_weather_city_storage {

inline constexpr const char *kManualWeatherCityKey = "weather_city_v1";
inline constexpr const char *kIgnoredAssetWeatherCityKey = "asset_city_skip";

bool load_preferred_city(nvs_handle_t nvs, char *out, size_t out_len);
esp_err_t write_provisioned_city(nvs_handle_t nvs,
                                 esp_err_t err,
                                 const char *city,
                                 bool *changed);
esp_err_t write_manual_city_if_changed(nvs_handle_t nvs,
                                       const char *city,
                                       bool *changed);
esp_err_t clear_manual_city(nvs_handle_t nvs,
                            const char *active_city,
                            bool *changed);

} // namespace network_weather_city_storage
