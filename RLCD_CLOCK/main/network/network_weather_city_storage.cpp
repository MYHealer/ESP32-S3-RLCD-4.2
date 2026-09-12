// 实现手动天气城市、上位机资源城市和忽略标记之间的持久化规则。
#include "network_weather_city_storage.h"

#include "app_metadata.h"
#include "app_text_format.h"
#include "custom_assets.h"
#include "network_config_nvs.h"
#include "weather_city_contract.h"
#include "weather_city_text.h"

#include <esp_log.h>
#include <string.h>

namespace network_weather_city_storage {
namespace {
constexpr const char *kInvalidWeatherCityLoadLog =
    "ignore invalid weather city loaded from NVS";

static_assert(kManualWeatherCityLen == kCustomAssetWeatherCityMaxLen + 1,
              "weather city buffers must cover the custom asset city limit");
static_assert(kInvalidWeatherCityLoadLog[0] != '\0',
              "invalid weather city warning must not be empty");

esp_err_t write_manual_city_key(nvs_handle_t nvs, const char *city)
{
    return network_config_nvs::write_optional_nvs_string_key(
        nvs, kManualWeatherCityKey, city);
}

esp_err_t write_ignored_asset_city(nvs_handle_t nvs, const char *city)
{
    return network_config_nvs::write_optional_nvs_string_key(
        nvs, kIgnoredAssetWeatherCityKey, city);
}

esp_err_t write_city_key_if_changed(nvs_handle_t nvs,
                                    esp_err_t err,
                                    const char *key,
                                    const char *city,
                                    bool *changed)
{
    char saved[kManualWeatherCityLen] = {};
    return network_config_nvs::write_changed_optional_nvs_string(
        nvs, err, key, city, saved, sizeof(saved), changed);
}

bool read_valid_asset_city(char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    out[0] = '\0';
    if (!custom_assets_read_weather_city(out, out_len)) {
        return false;
    }
    char normalized[kManualWeatherCityLen] = {};
    if (!weather_city_text::normalize(out, normalized, sizeof(normalized)) ||
        normalized[0] == '\0' || strlen(normalized) >= out_len) {
        return false;
    }
    memcpy(out, normalized, strlen(normalized) + 1);
    return true;
}

bool asset_city_ignored(nvs_handle_t nvs, const char *city)
{
    if (!city || city[0] == '\0') {
        return false;
    }
    char ignored[kManualWeatherCityLen] = {};
    return network_config_nvs::nvs_string_matches(nvs,
                                                  kIgnoredAssetWeatherCityKey,
                                                  city,
                                                  ignored,
                                                  sizeof(ignored));
}

bool read_unignored_asset_city(nvs_handle_t nvs, char *out, size_t out_len)
{
    return read_valid_asset_city(out, out_len) && !asset_city_ignored(nvs, out);
}

esp_err_t write_matching_asset_city_ignore(nvs_handle_t nvs,
                                           const char *city,
                                           bool *wrote)
{
    if (wrote) {
        *wrote = false;
    }
    char asset_city[kManualWeatherCityLen] = {};
    if (city && read_valid_asset_city(asset_city, sizeof(asset_city)) &&
        strcmp(asset_city, city) == 0) {
        esp_err_t err = write_ignored_asset_city(nvs, asset_city);
        if (err == ESP_OK && wrote) {
            *wrote = true;
        }
        return err;
    }
    return ESP_OK;
}
} // namespace

bool load_preferred_city(nvs_handle_t nvs, char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    out[0] = '\0';
    if (network_config_nvs::read_nvs_string(
            nvs, kManualWeatherCityKey, out, out_len) == ESP_OK) {
        char normalized[kManualWeatherCityLen] = {};
        if (!weather_city_text::normalize(out, normalized, sizeof(normalized))) {
            ESP_LOGW(TAG, "%s", kInvalidWeatherCityLoadLog);
            out[0] = '\0';
        } else {
            memcpy(out, normalized, strlen(normalized) + 1);
        }
    }
    if (out[0] == '\0') {
        char asset_city[kManualWeatherCityLen] = {};
        if (read_unignored_asset_city(nvs, asset_city, sizeof(asset_city)) &&
            strlen(asset_city) < out_len) {
            memcpy(out, asset_city, strlen(asset_city) + 1);
        }
    }
    return out[0] != '\0';
}

esp_err_t write_provisioned_city(nvs_handle_t nvs,
                                 esp_err_t err,
                                 const char *city,
                                 bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    bool manual_changed = false;
    err = write_city_key_if_changed(
        nvs, err, kManualWeatherCityKey, city, &manual_changed);
    if (err != ESP_OK) {
        return err;
    }
    char asset_city[kManualWeatherCityLen] = {};
    const char *ignored_city = nullptr;
    if ((!city || city[0] == '\0') &&
        read_valid_asset_city(asset_city, sizeof(asset_city))) {
        ignored_city = asset_city;
    }
    bool ignored_changed = false;
    err = write_city_key_if_changed(nvs,
                                    err,
                                    kIgnoredAssetWeatherCityKey,
                                    ignored_city,
                                    &ignored_changed);
    if (err == ESP_OK && changed) {
        *changed = manual_changed || ignored_changed;
    }
    return err;
}

esp_err_t write_manual_city_if_changed(nvs_handle_t nvs,
                                       const char *city,
                                       bool *changed)
{
    if (changed) {
        *changed = false;
    }
    char saved[kManualWeatherCityLen] = {};
    if (network_config_nvs::nvs_string_matches(
            nvs, kManualWeatherCityKey, city, saved, sizeof(saved))) {
        return network_config_nvs::erase_nvs_key_if_present(
            nvs, kIgnoredAssetWeatherCityKey, changed);
    }
    esp_err_t err = write_manual_city_key(nvs, city);
    if (err == ESP_OK) {
        err = write_ignored_asset_city(nvs, nullptr);
    }
    if (err == ESP_OK && changed) {
        *changed = true;
    }
    return err;
}

esp_err_t clear_manual_city(nvs_handle_t nvs,
                            const char *active_city,
                            bool *changed)
{
    if (changed) {
        *changed = false;
    }
    bool erased = false;
    esp_err_t err = network_config_nvs::erase_nvs_key_if_present(
        nvs, kManualWeatherCityKey, &erased);
    if (err == ESP_OK) {
        bool ignored_asset_city = false;
        err = write_matching_asset_city_ignore(
            nvs, active_city, &ignored_asset_city);
        erased = erased || ignored_asset_city;
    }
    if (err == ESP_OK && changed) {
        *changed = erased;
    }
    return err;
}

} // namespace network_weather_city_storage
