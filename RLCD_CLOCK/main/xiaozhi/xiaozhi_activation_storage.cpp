// 持久化小智绑定结果和设备 Client ID，不参与网络与语音状态机。
#include "xiaozhi_activation_storage.h"

#include "app_metadata.h"
#include "app_text_format.h"
#include "scoped_nvs_handle.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>
#include <nvs.h>

#include <cstdio>
#include <cstring>

namespace {
using app_storage::ScopedNvsHandle;

constexpr const char *kNvsNamespace = "xiaozhi";
constexpr const char *kWebsocketUrlKey = "ws_url";
constexpr const char *kWebsocketTokenKey = "ws_token";
constexpr const char *kWebsocketVersionKey = "ws_ver";
constexpr const char *kActivationChallengeKey = "act_chal";
constexpr const char *kClientIdKey = "client_id";
constexpr const char *kBindingConfirmedKey = "bound_v1";

#define XIAOZHI_NVS_CLEAR_OPEN_FAILED_FORMAT "xiaozhi activation NVS open for clear failed: %s"
#define XIAOZHI_NVS_CLEAR_ERASE_FAILED_FORMAT "xiaozhi activation NVS erase failed: %s"
#define XIAOZHI_NVS_CLEAR_COMMIT_FAILED_FORMAT "xiaozhi activation NVS clear commit failed: %s"
#define XIAOZHI_ACTIVATION_NVS_OPEN_FAILED_FORMAT "xiaozhi activation NVS open failed: %s"
#define XIAOZHI_ACTIVATION_NVS_SAVE_FAILED_FORMAT "xiaozhi activation NVS save failed: %s"
#define XIAOZHI_CLIENT_ID_NVS_OPEN_FAILED_FORMAT "xiaozhi client id NVS open failed: %s"
#define XIAOZHI_CLIENT_ID_NVS_SAVE_FAILED_FORMAT "xiaozhi client id NVS save failed: %s"

static_assert(kXiaozhiClientIdSize == sizeof("00000000-0000-0000-0000-000000000000"),
              "Xiaozhi client ID buffer must fit one UUID plus terminator");

bool nvs_read_string(nvs_handle_t nvs, const char *key, char *out, size_t out_len)
{
    if (!key || !app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    size_t len = out_len;
    if (nvs_get_str(nvs, key, out, &len) != ESP_OK || out[0] == '\0') {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool nvs_string_equals(nvs_handle_t nvs, const char *key, const char *expected)
{
    if (!key || !expected || strlen(expected) >= kXiaozhiWebsocketConfigFieldSize) {
        return false;
    }
    char stored[kXiaozhiWebsocketConfigFieldSize] = {};
    size_t stored_len = sizeof(stored);
    return nvs_get_str(nvs, key, stored, &stored_len) == ESP_OK &&
           strcmp(stored, expected) == 0;
}

esp_err_t write_nvs_string_if_changed(nvs_handle_t nvs,
                                      esp_err_t err,
                                      const char *key,
                                      const char *value,
                                      bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (nvs_string_equals(nvs, key, value)) {
        return ESP_OK;
    }
    err = nvs_set_str(nvs, key, value);
    if (err == ESP_OK && changed) {
        *changed = true;
    }
    return err;
}

esp_err_t write_nvs_i32_if_changed(nvs_handle_t nvs,
                                   esp_err_t err,
                                   const char *key,
                                   int32_t value,
                                   bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    int32_t stored = 0;
    if (nvs_get_i32(nvs, key, &stored) == ESP_OK && stored == value) {
        return ESP_OK;
    }
    err = nvs_set_i32(nvs, key, value);
    if (err == ESP_OK && changed) {
        *changed = true;
    }
    return err;
}

esp_err_t write_nvs_u8_if_changed(nvs_handle_t nvs,
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
    uint8_t stored = 0;
    if (nvs_get_u8(nvs, key, &stored) == ESP_OK && stored == value) {
        return ESP_OK;
    }
    err = nvs_set_u8(nvs, key, value);
    if (err == ESP_OK && changed) {
        *changed = true;
    }
    return err;
}
} // namespace

bool xiaozhi_load_websocket_config(char *url,
                                    size_t url_len,
                                    char *token,
                                    size_t token_len,
                                    int32_t *version)
{
    if (!app_text::output_buffer_available(url, url_len) ||
        !app_text::output_buffer_available(token, token_len) || !version) {
        return false;
    }
    ScopedNvsHandle nvs;
    if (nvs.open(kNvsNamespace, NVS_READONLY) != ESP_OK) {
        return false;
    }
    uint8_t binding_confirmed = 0;
    bool present = nvs_get_u8(nvs.get(), kBindingConfirmedKey, &binding_confirmed) == ESP_OK &&
                   binding_confirmed == 1 &&
                   nvs_read_string(nvs.get(), kWebsocketUrlKey, url, url_len);
    nvs_read_string(nvs.get(), kWebsocketTokenKey, token, token_len);
    if (nvs_get_i32(nvs.get(), kWebsocketVersionKey, version) != ESP_OK || *version <= 0) {
        *version = 1;
    }
    nvs.close();
    return present;
}

bool xiaozhi_save_activation_config(cJSON *websocket, const char *challenge)
{
    if (!cJSON_IsObject(websocket)) {
        return false;
    }
    cJSON *url = cJSON_GetObjectItem(websocket, "url");
    cJSON *token = cJSON_GetObjectItem(websocket, "token");
    cJSON *version = cJSON_GetObjectItem(websocket, "version");
    if (!cJSON_IsString(url) || !url->valuestring || url->valuestring[0] == '\0') {
        return false;
    }
    const char *token_value =
        cJSON_IsString(token) && token->valuestring ? token->valuestring : nullptr;
    const int32_t version_value = cJSON_IsNumber(version) ? version->valueint : 1;
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNvsNamespace, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_ACTIVATION_NVS_OPEN_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    bool changed = false;
    bool item_changed = false;
    err = write_nvs_string_if_changed(nvs.get(),
                                      err,
                                      kWebsocketUrlKey,
                                      url->valuestring,
                                      &item_changed);
    changed = changed || item_changed;
    if (token_value) {
        err = write_nvs_string_if_changed(nvs.get(),
                                          err,
                                          kWebsocketTokenKey,
                                          token_value,
                                          &item_changed);
        changed = changed || item_changed;
    }
    err = write_nvs_i32_if_changed(nvs.get(),
                                   err,
                                   kWebsocketVersionKey,
                                   version_value,
                                   &item_changed);
    changed = changed || item_changed;
    err = write_nvs_u8_if_changed(nvs.get(),
                                  err,
                                  kBindingConfirmedKey,
                                  1,
                                  &item_changed);
    changed = changed || item_changed;
    if (challenge && challenge[0] != '\0') {
        err = write_nvs_string_if_changed(nvs.get(),
                                          err,
                                          kActivationChallengeKey,
                                          challenge,
                                          &item_changed);
        changed = changed || item_changed;
    }
    if (err == ESP_OK && changed) {
        err = nvs_commit(nvs.get());
    }
    nvs.close();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_ACTIVATION_NVS_SAVE_FAILED_FORMAT, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

bool xiaozhi_load_or_create_client_id(char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len) || out_len < kXiaozhiClientIdSize) {
        return false;
    }
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNvsNamespace, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_CLIENT_ID_NVS_OPEN_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    size_t stored_len = out_len;
    if (nvs_get_str(nvs.get(), kClientIdKey, out, &stored_len) == ESP_OK &&
        strlen(out) == kXiaozhiClientIdSize - 1) {
        nvs.close();
        return true;
    }
    uint8_t uuid[16] = {};
    esp_fill_random(uuid, sizeof(uuid));
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    snprintf(out,
             out_len,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3],
             uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9], uuid[10], uuid[11],
             uuid[12], uuid[13], uuid[14], uuid[15]);
    err = nvs_set_str(nvs.get(), kClientIdKey, out);
    if (err == ESP_OK) {
        err = nvs_commit(nvs.get());
    }
    nvs.close();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_CLIENT_ID_NVS_SAVE_FAILED_FORMAT, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

bool xiaozhi_clear_activation_storage()
{
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNvsNamespace, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_NVS_CLEAR_OPEN_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    err = nvs_erase_all(nvs.get());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_NVS_CLEAR_ERASE_FAILED_FORMAT, esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs.get());
        if (err != ESP_OK) {
            ESP_LOGW(TAG, XIAOZHI_NVS_CLEAR_COMMIT_FAILED_FORMAT, esp_err_to_name(err));
        }
    }
    nvs.close();
    return err == ESP_OK;
}
