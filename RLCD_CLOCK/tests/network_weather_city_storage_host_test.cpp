// 验证手动城市、上位机城市和忽略标记的 NVS 优先级及条件写入规则。
#include "network_weather_city_storage.h"

#include "custom_assets.h"

#include <assert.h>
#include <map>
#include <string>
#include <string.h>

namespace {
std::map<std::string, std::string> g_values;
std::string g_asset_city;
int g_get_calls = 0;
int g_set_calls = 0;
int g_erase_calls = 0;
int g_fail_set_call = -1;
int g_fail_erase_call = -1;

void reset_store()
{
    g_values.clear();
    g_asset_city.clear();
    g_get_calls = 0;
    g_set_calls = 0;
    g_erase_calls = 0;
    g_fail_set_call = -1;
    g_fail_erase_call = -1;
}

void expect_value(const char *key, const char *expected)
{
    auto found = g_values.find(key);
    assert(found != g_values.end());
    assert(found->second == expected);
}
} // namespace

const char *esp_err_to_name(esp_err_t)
{
    return "host";
}

esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *)
{
    return ESP_FAIL;
}

void nvs_close(nvs_handle_t) {}

esp_err_t nvs_commit(nvs_handle_t)
{
    return ESP_OK;
}

esp_err_t nvs_get_str(nvs_handle_t, const char *key, char *out, size_t *len)
{
    ++g_get_calls;
    if (!key || !len) {
        return ESP_ERR_INVALID_ARG;
    }
    auto found = g_values.find(key);
    if (found == g_values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    size_t required = found->second.size() + 1;
    if (!out || *len < required) {
        *len = required;
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out, found->second.c_str(), required);
    *len = required;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t, const char *key, const char *value)
{
    ++g_set_calls;
    if (g_fail_set_call == g_set_calls) {
        return ESP_FAIL;
    }
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    g_values[key] = value;
    return ESP_OK;
}

esp_err_t nvs_erase_key(nvs_handle_t, const char *key)
{
    ++g_erase_calls;
    if (g_fail_erase_call == g_erase_calls) {
        return ESP_FAIL;
    }
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    return g_values.erase(key) > 0 ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t)
{
    return ESP_FAIL;
}

esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *)
{
    return ESP_ERR_NVS_NOT_FOUND;
}

bool custom_assets_read_weather_city(char *out, size_t out_len)
{
    if (!out || out_len == 0 || g_asset_city.empty() ||
        g_asset_city.size() >= out_len) {
        return false;
    }
    memcpy(out, g_asset_city.c_str(), g_asset_city.size() + 1);
    return true;
}

int main()
{
    constexpr nvs_handle_t kNvs = 1;
    char city[kCustomAssetWeatherCityMaxLen + 1] = {};

    reset_store();
    assert(!network_weather_city_storage::load_preferred_city(
        kNvs, city, sizeof(city)));
    assert(city[0] == '\0');

    reset_store();
    g_asset_city = " 杭州市 ";
    assert(network_weather_city_storage::load_preferred_city(
        kNvs, city, sizeof(city)));
    assert(strcmp(city, "杭州") == 0);

    reset_store();
    g_asset_city = "杭州市";
    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "杭州";
    assert(!network_weather_city_storage::load_preferred_city(
        kNvs, city, sizeof(city)));
    assert(city[0] == '\0');

    reset_store();
    g_asset_city = "杭州";
    g_values[network_weather_city_storage::kManualWeatherCityKey] = " 上海市 ";
    assert(network_weather_city_storage::load_preferred_city(
        kNvs, city, sizeof(city)));
    assert(strcmp(city, "上海") == 0);

    reset_store();
    g_asset_city = "南京市";
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "杭州/上海";
    assert(network_weather_city_storage::load_preferred_city(
        kNvs, city, sizeof(city)));
    assert(strcmp(city, "南京") == 0);

    reset_store();
    g_asset_city = "杭州";
    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "杭州";
    bool changed = false;
    assert(network_weather_city_storage::write_provisioned_city(
               kNvs, ESP_OK, "上海", &changed) == ESP_OK);
    assert(changed);
    expect_value(network_weather_city_storage::kManualWeatherCityKey, "上海");
    assert(g_values.count(
               network_weather_city_storage::kIgnoredAssetWeatherCityKey) == 0);

    reset_store();
    g_asset_city = "杭州市";
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "上海";
    changed = false;
    assert(network_weather_city_storage::write_provisioned_city(
               kNvs, ESP_OK, "", &changed) == ESP_OK);
    assert(changed);
    assert(g_values.count(network_weather_city_storage::kManualWeatherCityKey) == 0);
    expect_value(network_weather_city_storage::kIgnoredAssetWeatherCityKey, "杭州");

    reset_store();
    changed = true;
    assert(network_weather_city_storage::write_provisioned_city(
               kNvs, ESP_FAIL, "上海", &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_get_calls == 0 && g_set_calls == 0 && g_erase_calls == 0);

    reset_store();
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "上海";
    changed = true;
    assert(network_weather_city_storage::write_provisioned_city(
               kNvs, ESP_OK, "上海", &changed) == ESP_OK);
    assert(!changed);
    assert(g_set_calls == 0 && g_erase_calls == 1);

    reset_store();
    g_asset_city = "杭州市";
    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "杭州";
    changed = true;
    assert(network_weather_city_storage::write_provisioned_city(
               kNvs, ESP_OK, "", &changed) == ESP_OK);
    assert(!changed);
    assert(g_set_calls == 0 && g_erase_calls == 1);

    reset_store();
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "杭州";
    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "杭州";
    changed = false;
    assert(network_weather_city_storage::write_manual_city_if_changed(
               kNvs, "杭州", &changed) == ESP_OK);
    assert(changed);
    assert(g_set_calls == 0);
    assert(g_values.count(
               network_weather_city_storage::kIgnoredAssetWeatherCityKey) == 0);

    changed = true;
    assert(network_weather_city_storage::write_manual_city_if_changed(
               kNvs, "杭州", &changed) == ESP_OK);
    assert(!changed);
    assert(g_set_calls == 0);

    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "南京";
    assert(network_weather_city_storage::write_manual_city_if_changed(
               kNvs, "上海", &changed) == ESP_OK);
    assert(changed);
    expect_value(network_weather_city_storage::kManualWeatherCityKey, "上海");
    assert(g_values.count(
               network_weather_city_storage::kIgnoredAssetWeatherCityKey) == 0);

    reset_store();
    g_asset_city = "杭州市";
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "杭州";
    assert(network_weather_city_storage::clear_manual_city(
               kNvs, "杭州", &changed) == ESP_OK);
    assert(changed);
    assert(g_values.count(network_weather_city_storage::kManualWeatherCityKey) == 0);
    expect_value(network_weather_city_storage::kIgnoredAssetWeatherCityKey, "杭州");

    reset_store();
    g_asset_city = "杭州";
    changed = true;
    assert(network_weather_city_storage::clear_manual_city(
               kNvs, "上海", &changed) == ESP_OK);
    assert(!changed);
    assert(g_values.empty());

    reset_store();
    g_fail_set_call = 1;
    changed = false;
    assert(network_weather_city_storage::write_manual_city_if_changed(
               kNvs, "上海", &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_set_calls == 1);
    assert(g_erase_calls == 0);

    reset_store();
    g_values[network_weather_city_storage::kIgnoredAssetWeatherCityKey] = "杭州";
    g_fail_erase_call = 1;
    changed = true;
    assert(network_weather_city_storage::write_manual_city_if_changed(
               kNvs, "上海", &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_set_calls == 1);
    assert(g_erase_calls == 1);

    reset_store();
    g_asset_city = "杭州";
    g_values[network_weather_city_storage::kManualWeatherCityKey] = "杭州";
    g_fail_set_call = 1;
    changed = true;
    assert(network_weather_city_storage::clear_manual_city(
               kNvs, "杭州", &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_erase_calls == 1);
    assert(g_set_calls == 1);
    return 0;
}
