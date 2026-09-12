// 实现联网配置 NVS 原语，不包含页面、天气城市或离线模式业务规则。
#include "network_config_nvs.h"

#include "app_metadata.h"

#include <esp_log.h>

#include <string.h>

namespace network_config_nvs {
namespace {
constexpr const char *kWifiNvsNamespace = "wifi";
constexpr const char *kDefaultAction = "accessing config";
constexpr uint8_t kUnsetU8 = 0xff;
#define NVS_OPEN_FAILED_FORMAT "nvs open failed while %s: %s"
#define NVS_READ_U8_FAILED_FORMAT "nvs read u8 key=%s failed: %s"

static_assert(kWifiNvsNamespace[0] != '\0', "Wi-Fi NVS namespace must not be empty");
static_assert(kDefaultAction[0] != '\0', "default NVS action must not be empty");

const char *action_or_default(const char *action)
{
    return action && action[0] != '\0' ? action : kDefaultAction;
}
} // namespace

esp_err_t erase_nvs_key_if_present(nvs_handle_t nvs, const char *key, bool *erased)
{
    if (erased) {
        *erased = false;
    }
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = nvs_erase_key(nvs, key);
    if (err == ESP_OK) {
        if (erased) {
            *erased = true;
        }
        return ESP_OK;
    }
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

esp_err_t open_wifi_nvs(nvs_open_mode_t mode,
                        nvs_handle_t *nvs,
                        const char *action,
                        bool log_not_found)
{
    if (!nvs) {
        return ESP_ERR_INVALID_ARG;
    }
    *nvs = 0;
    esp_err_t err = nvs_open(kWifiNvsNamespace, mode, nvs);
    if (err != ESP_OK && (log_not_found || err != ESP_ERR_NVS_NOT_FOUND)) {
        ESP_LOGW(TAG, NVS_OPEN_FAILED_FORMAT, action_or_default(action), esp_err_to_name(err));
    }
    return err;
}

esp_err_t commit_nvs_if_ok(nvs_handle_t nvs, esp_err_t err)
{
    return err == ESP_OK ? nvs_commit(nvs) : err;
}

esp_err_t commit_nvs_if_changed(nvs_handle_t nvs, esp_err_t err, bool changed)
{
    return changed ? commit_nvs_if_ok(nvs, err) : err;
}

esp_err_t set_nvs_u8_if_ok(nvs_handle_t nvs,
                           esp_err_t err,
                           const char *key,
                           uint8_t value)
{
    if (err != ESP_OK) {
        return err;
    }
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_u8(nvs, key, value);
}

esp_err_t write_optional_nvs_string_key(nvs_handle_t nvs, const char *key, const char *value)
{
    return value && value[0] != '\0'
               ? nvs_set_str(nvs, key, value)
               : erase_nvs_key_if_present(nvs, key, nullptr);
}

esp_err_t write_changed_nvs_string(nvs_handle_t nvs,
                                   esp_err_t err,
                                   const char *key,
                                   const char *value,
                                   char *scratch,
                                   size_t scratch_len,
                                   bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (!key || !value || !scratch || scratch_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (read_nvs_string(nvs, key, scratch, scratch_len) == ESP_OK &&
        strcmp(scratch, value) == 0) {
        return ESP_OK;
    }
    esp_err_t write_err = nvs_set_str(nvs, key, value);
    if (write_err == ESP_OK && changed) {
        *changed = true;
    }
    return write_err;
}

esp_err_t write_changed_optional_nvs_string(nvs_handle_t nvs,
                                            esp_err_t err,
                                            const char *key,
                                            const char *value,
                                            char *scratch,
                                            size_t scratch_len,
                                            bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    return value && value[0] != '\0'
               ? write_changed_nvs_string(
                     nvs, err, key, value, scratch, scratch_len, changed)
               : erase_nvs_key_if_present(nvs, key, changed);
}

esp_err_t read_nvs_string(nvs_handle_t nvs, const char *key, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = out_len;
    esp_err_t err = nvs_get_str(nvs, key, out, &len);
    if (err != ESP_OK) {
        out[0] = '\0';
    }
    return err;
}

bool nvs_string_matches(nvs_handle_t nvs,
                        const char *key,
                        const char *expected,
                        char *scratch,
                        size_t scratch_len)
{
    if (!expected || expected[0] == '\0' || !scratch || scratch_len == 0) {
        return false;
    }
    scratch[0] = '\0';
    return read_nvs_string(nvs, key, scratch, scratch_len) == ESP_OK &&
           strcmp(scratch, expected) == 0;
}

uint8_t read_nvs_u8_or_default(nvs_handle_t nvs, const char *key, uint8_t default_value)
{
    if (!key) {
        return default_value;
    }
    uint8_t value = default_value;
    esp_err_t err = nvs_get_u8(nvs, key, &value);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, NVS_READ_U8_FAILED_FORMAT, key, esp_err_to_name(err));
    }
    return value;
}

bool nvs_u8_matches(nvs_handle_t nvs, const char *key, uint8_t expected)
{
    if (!key) {
        return false;
    }
    uint8_t value = kUnsetU8;
    return nvs_get_u8(nvs, key, &value) == ESP_OK && value == expected;
}

esp_err_t write_changed_nvs_u8(nvs_handle_t nvs,
                               esp_err_t err,
                               const char *key,
                               uint8_t value,
                               bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (nvs_u8_matches(nvs, key, value)) {
        return ESP_OK;
    }
    esp_err_t write_err = set_nvs_u8_if_ok(nvs, err, key, value);
    if (write_err == ESP_OK && changed) {
        *changed = true;
    }
    return write_err;
}

} // namespace network_config_nvs
