// 验证联网配置 NVS 原语在成功和失败路径中提供确定的输出状态。
#include "network_config_nvs.h"

#include <assert.h>
#include <string.h>

namespace {
esp_err_t g_open_result = ESP_OK;
nvs_handle_t g_open_handle = 17;
int g_open_calls = 0;

esp_err_t g_erase_result = ESP_OK;
int g_erase_calls = 0;

esp_err_t g_get_string_result = ESP_OK;
const char *g_stored_string = "stored";
int g_get_string_calls = 0;
esp_err_t g_set_string_result = ESP_OK;
int g_set_string_calls = 0;

esp_err_t g_get_u8_result = ESP_ERR_NVS_NOT_FOUND;
uint8_t g_stored_u8 = 0;
int g_get_u8_calls = 0;

esp_err_t g_set_u8_result = ESP_OK;
int g_set_u8_calls = 0;

void reset_state()
{
    g_open_result = ESP_OK;
    g_open_handle = 17;
    g_open_calls = 0;
    g_erase_result = ESP_OK;
    g_erase_calls = 0;
    g_get_string_result = ESP_OK;
    g_stored_string = "stored";
    g_get_string_calls = 0;
    g_set_string_result = ESP_OK;
    g_set_string_calls = 0;
    g_get_u8_result = ESP_ERR_NVS_NOT_FOUND;
    g_stored_u8 = 0;
    g_get_u8_calls = 0;
    g_set_u8_result = ESP_OK;
    g_set_u8_calls = 0;
}
} // namespace

const char *esp_err_to_name(esp_err_t)
{
    return "host";
}

esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *handle)
{
    ++g_open_calls;
    if (g_open_result == ESP_OK && handle) {
        *handle = g_open_handle;
    }
    return g_open_result;
}

void nvs_close(nvs_handle_t) {}

esp_err_t nvs_erase_key(nvs_handle_t, const char *)
{
    ++g_erase_calls;
    return g_erase_result;
}

esp_err_t nvs_commit(nvs_handle_t)
{
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *)
{
    ++g_set_string_calls;
    return g_set_string_result;
}

esp_err_t nvs_get_str(nvs_handle_t, const char *, char *out, size_t *len)
{
    ++g_get_string_calls;
    if (g_get_string_result != ESP_OK) {
        if (out && len && *len > 1) {
            out[0] = 'X';
            out[1] = '\0';
        }
        return g_get_string_result;
    }
    size_t required = strlen(g_stored_string) + 1;
    if (!out || !len || *len < required) {
        return ESP_FAIL;
    }
    memcpy(out, g_stored_string, required);
    *len = required;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *value)
{
    ++g_get_u8_calls;
    if (g_get_u8_result == ESP_OK && value) {
        *value = g_stored_u8;
    }
    return g_get_u8_result;
}

esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t)
{
    ++g_set_u8_calls;
    return g_set_u8_result;
}

int main()
{
    reset_state();
    assert(network_config_nvs::open_wifi_nvs(NVS_READONLY, nullptr, "test") ==
           ESP_ERR_INVALID_ARG);
    assert(g_open_calls == 0);

    nvs_handle_t handle = 99;
    g_open_result = ESP_FAIL;
    assert(network_config_nvs::open_wifi_nvs(NVS_READONLY, &handle, "test") == ESP_FAIL);
    assert(handle == 0);
    assert(g_open_calls == 1);

    g_open_result = ESP_OK;
    g_open_handle = 42;
    handle = 99;
    assert(network_config_nvs::open_wifi_nvs(NVS_READWRITE, &handle, "test") == ESP_OK);
    assert(handle == 42);

    bool erased = true;
    assert(network_config_nvs::erase_nvs_key_if_present(1, nullptr, &erased) ==
           ESP_ERR_INVALID_ARG);
    assert(!erased);
    assert(g_erase_calls == 0);

    erased = true;
    g_erase_result = ESP_ERR_NVS_NOT_FOUND;
    assert(network_config_nvs::erase_nvs_key_if_present(1, "missing", &erased) == ESP_OK);
    assert(!erased);

    erased = true;
    g_erase_result = ESP_FAIL;
    assert(network_config_nvs::erase_nvs_key_if_present(1, "failed", &erased) == ESP_FAIL);
    assert(!erased);

    g_erase_result = ESP_OK;
    assert(network_config_nvs::erase_nvs_key_if_present(1, "saved", &erased) == ESP_OK);
    assert(erased);

    char text[16] = "stale";
    assert(network_config_nvs::read_nvs_string(1, nullptr, text, sizeof(text)) ==
           ESP_ERR_INVALID_ARG);
    assert(text[0] == '\0');
    assert(g_get_string_calls == 0);

    g_get_string_result = ESP_FAIL;
    memcpy(text, "stale", 6);
    assert(network_config_nvs::read_nvs_string(1, "name", text, sizeof(text)) == ESP_FAIL);
    assert(text[0] == '\0');

    g_get_string_result = ESP_OK;
    assert(network_config_nvs::read_nvs_string(1, "name", text, sizeof(text)) == ESP_OK);
    assert(strcmp(text, "stored") == 0);

    bool changed = true;
    assert(network_config_nvs::write_changed_nvs_string(
               1, ESP_FAIL, "name", "next", text, sizeof(text), &changed) == ESP_FAIL);
    assert(!changed && g_get_string_calls == 2 && g_set_string_calls == 0);

    changed = true;
    assert(network_config_nvs::write_changed_nvs_string(
               1, ESP_OK, "name", "stored", text, sizeof(text), &changed) == ESP_OK);
    assert(!changed && g_set_string_calls == 0);

    g_set_string_result = ESP_FAIL;
    changed = true;
    assert(network_config_nvs::write_changed_nvs_string(
               1, ESP_OK, "name", "next", text, sizeof(text), &changed) == ESP_FAIL);
    assert(!changed && g_set_string_calls == 1);

    g_set_string_result = ESP_OK;
    assert(network_config_nvs::write_changed_nvs_string(
               1, ESP_OK, "name", "next", text, sizeof(text), &changed) == ESP_OK);
    assert(changed && g_set_string_calls == 2);

    g_erase_result = ESP_ERR_NVS_NOT_FOUND;
    changed = true;
    assert(network_config_nvs::write_changed_optional_nvs_string(
               1, ESP_OK, "name", "", text, sizeof(text), &changed) == ESP_OK);
    assert(!changed);

    g_erase_result = ESP_OK;
    assert(network_config_nvs::write_changed_optional_nvs_string(
               1, ESP_OK, "name", nullptr, text, sizeof(text), &changed) == ESP_OK);
    assert(changed);

    changed = true;
    assert(network_config_nvs::write_changed_nvs_u8(1, ESP_FAIL, "value", 7, &changed) ==
           ESP_FAIL);
    assert(!changed);
    assert(g_get_u8_calls == 0);
    assert(g_set_u8_calls == 0);

    g_get_u8_result = ESP_OK;
    g_stored_u8 = 7;
    changed = true;
    assert(network_config_nvs::write_changed_nvs_u8(1, ESP_OK, "value", 7, &changed) ==
           ESP_OK);
    assert(!changed);
    assert(g_set_u8_calls == 0);

    g_stored_u8 = 3;
    g_set_u8_result = ESP_FAIL;
    changed = true;
    assert(network_config_nvs::write_changed_nvs_u8(1, ESP_OK, "value", 7, &changed) ==
           ESP_FAIL);
    assert(!changed);
    assert(g_set_u8_calls == 1);

    g_set_u8_result = ESP_OK;
    assert(network_config_nvs::write_changed_nvs_u8(1, ESP_OK, "value", 7, &changed) ==
           ESP_OK);
    assert(changed);
    assert(g_set_u8_calls == 2);
    return 0;
}
