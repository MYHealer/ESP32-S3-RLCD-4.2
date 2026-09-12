// 验证小智激活配置和 Client ID 的 NVS 读写、错误及兼容语义。
#include "xiaozhi_activation_storage.h"

#include "cJSON.h"
#include "nvs.h"

#include <assert.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr nvs_handle_t kHandle = 17;

std::map<std::string, std::string> g_strings;
std::map<std::string, int32_t> g_i32_values;
std::map<std::string, uint8_t> g_u8_values;
std::vector<std::string> g_operations;
esp_err_t g_open_error = ESP_OK;
esp_err_t g_commit_error = ESP_OK;
esp_err_t g_erase_error = ESP_OK;
std::string g_failed_set_key;
int g_open_calls = 0;
int g_close_calls = 0;

void reset_store()
{
    g_strings.clear();
    g_i32_values.clear();
    g_u8_values.clear();
    g_operations.clear();
    g_open_error = ESP_OK;
    g_commit_error = ESP_OK;
    g_erase_error = ESP_OK;
    g_failed_set_key.clear();
    g_open_calls = 0;
    g_close_calls = 0;
}

bool set_fails(const char *key)
{
    return !g_failed_set_key.empty() && g_failed_set_key == key;
}

cJSON *parse_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    assert(root);
    return root;
}
} // namespace

esp_err_t nvs_open(const char *name, nvs_open_mode_t, nvs_handle_t *out)
{
    assert(name && std::string(name) == "xiaozhi");
    ++g_open_calls;
    if (g_open_error != ESP_OK) {
        return g_open_error;
    }
    *out = kHandle;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    assert(handle == kHandle);
    ++g_close_calls;
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out, size_t *len)
{
    assert(handle == kHandle && key && out && len);
    auto found = g_strings.find(key);
    if (found == g_strings.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    const size_t required = found->second.size() + 1;
    if (*len < required) {
        *len = required;
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out, found->second.c_str(), required);
    *len = required;
    return ESP_OK;
}

esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out)
{
    assert(handle == kHandle && key && out);
    auto found = g_i32_values.find(key);
    if (found == g_i32_values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out = found->second;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out)
{
    assert(handle == kHandle && key && out);
    auto found = g_u8_values.find(key);
    if (found == g_u8_values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out = found->second;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    assert(handle == kHandle && key && value);
    g_operations.emplace_back(std::string("str:") + key);
    if (set_fails(key)) {
        return ESP_FAIL;
    }
    g_strings[key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value)
{
    assert(handle == kHandle && key);
    g_operations.emplace_back(std::string("i32:") + key);
    if (set_fails(key)) {
        return ESP_FAIL;
    }
    g_i32_values[key] = value;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    assert(handle == kHandle && key);
    g_operations.emplace_back(std::string("u8:") + key);
    if (set_fails(key)) {
        return ESP_FAIL;
    }
    g_u8_values[key] = value;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == kHandle);
    g_operations.emplace_back("commit");
    return g_commit_error;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    assert(handle == kHandle);
    g_operations.emplace_back("erase_all");
    if (g_erase_error == ESP_OK) {
        g_strings.clear();
        g_i32_values.clear();
        g_u8_values.clear();
    }
    return g_erase_error;
}

void esp_fill_random(void *buffer, size_t len)
{
    auto *bytes = static_cast<uint8_t *>(buffer);
    for (size_t index = 0; index < len; ++index) {
        bytes[index] = static_cast<uint8_t>(index);
    }
}

int main()
{
    char url[64] = {};
    char token[64] = {};
    int32_t version = 0;

    reset_store();
    assert(!xiaozhi_load_websocket_config(nullptr, sizeof(url), token, sizeof(token), &version));
    assert(!xiaozhi_load_websocket_config(url, 0, token, sizeof(token), &version));
    assert(!xiaozhi_load_websocket_config(url, sizeof(url), nullptr, sizeof(token), &version));
    assert(!xiaozhi_load_websocket_config(url, sizeof(url), token, 0, &version));
    assert(!xiaozhi_load_websocket_config(url, sizeof(url), token, sizeof(token), nullptr));
    assert(g_open_calls == 0);

    reset_store();
    g_u8_values["bound_v1"] = 1;
    g_strings["ws_url"] = "wss://example.test/ws";
    g_strings["ws_token"] = "secret";
    g_i32_values["ws_ver"] = 3;
    assert(xiaozhi_load_websocket_config(url, sizeof(url), token, sizeof(token), &version));
    assert(strcmp(url, "wss://example.test/ws") == 0);
    assert(strcmp(token, "secret") == 0 && version == 3);
    assert(g_open_calls == 1 && g_close_calls == 1);

    reset_store();
    g_u8_values["bound_v1"] = 1;
    g_strings["ws_url"] = "wss://example.test/ws";
    g_i32_values["ws_ver"] = 0;
    assert(xiaozhi_load_websocket_config(url, sizeof(url), token, sizeof(token), &version));
    assert(token[0] == '\0' && version == 1);

    reset_store();
    cJSON *root = parse_json("{}");
    assert(!xiaozhi_save_activation_config(root, "challenge"));
    assert(g_open_calls == 0);
    cJSON_Delete(root);

    reset_store();
    root = parse_json("{\"url\":\"wss://example.test/ws\",\"token\":\"secret\",\"version\":2}");
    assert(xiaozhi_save_activation_config(root, "challenge"));
    assert((g_operations == std::vector<std::string>{
        "str:ws_url", "str:ws_token", "i32:ws_ver", "u8:bound_v1",
        "str:act_chal", "commit"}));
    assert(g_strings["ws_url"] == "wss://example.test/ws");
    assert(g_strings["ws_token"] == "secret");
    assert(g_strings["act_chal"] == "challenge");
    assert(g_i32_values["ws_ver"] == 2 && g_u8_values["bound_v1"] == 1);
    assert(g_close_calls == 1);

    g_operations.clear();
    g_open_calls = 0;
    g_close_calls = 0;
    assert(xiaozhi_save_activation_config(root, "challenge"));
    assert(g_operations.empty());
    assert(g_open_calls == 1 && g_close_calls == 1);
    cJSON_Delete(root);

    reset_store();
    g_strings["ws_token"] = "old-token";
    root = parse_json("{\"url\":\"ws://example.test/ws\"}");
    assert(xiaozhi_save_activation_config(root, ""));
    assert(g_strings["ws_token"] == "old-token");
    assert(g_i32_values["ws_ver"] == 1);
    assert((g_operations == std::vector<std::string>{
        "str:ws_url", "i32:ws_ver", "u8:bound_v1", "commit"}));
    cJSON_Delete(root);

    reset_store();
    g_strings["ws_url"] = "ws://example.test/ws";
    g_strings["ws_token"] = "old-token";
    g_strings["act_chal"] = "old-challenge";
    g_i32_values["ws_ver"] = 1;
    g_u8_values["bound_v1"] = 1;
    root = parse_json("{\"url\":\"ws://example.test/ws\"}");
    assert(xiaozhi_save_activation_config(root, ""));
    assert(g_operations.empty());
    assert(g_strings["ws_token"] == "old-token");
    assert(g_strings["act_chal"] == "old-challenge");
    assert(g_close_calls == 1);
    cJSON_Delete(root);

    reset_store();
    g_strings["ws_url"] = "wss://example.test/ws";
    g_strings["ws_token"] = "secret";
    g_strings["act_chal"] = "old-challenge";
    g_i32_values["ws_ver"] = 2;
    g_u8_values["bound_v1"] = 1;
    root = parse_json("{\"url\":\"wss://example.test/ws\",\"token\":\"secret\",\"version\":2}");
    assert(xiaozhi_save_activation_config(root, "new-challenge"));
    assert((g_operations == std::vector<std::string>{
        "str:act_chal", "commit"}));
    assert(g_strings["act_chal"] == "new-challenge");
    cJSON_Delete(root);

    reset_store();
    g_strings["ws_url"] = "wss://example.test/ws";
    g_strings["ws_token"] = "";
    g_i32_values["ws_ver"] = 1;
    g_u8_values["bound_v1"] = 1;
    root = parse_json("{\"url\":\"wss://example.test/ws\",\"token\":\"\"}");
    assert(xiaozhi_save_activation_config(root, nullptr));
    assert(g_operations.empty());
    assert(g_close_calls == 1);
    cJSON_Delete(root);

    reset_store();
    g_failed_set_key = "ws_ver";
    root = parse_json("{\"url\":\"wss://example.test/ws\",\"token\":\"secret\",\"version\":2}");
    assert(!xiaozhi_save_activation_config(root, "challenge"));
    assert((g_operations == std::vector<std::string>{
        "str:ws_url", "str:ws_token", "i32:ws_ver"}));
    assert(g_close_calls == 1);
    cJSON_Delete(root);

    char client_id[kXiaozhiClientIdSize] = {};
    reset_store();
    assert(!xiaozhi_load_or_create_client_id(client_id, sizeof(client_id) - 1));
    assert(g_open_calls == 0);

    reset_store();
    g_strings["client_id"] = "12345678-1234-1234-1234-123456789abc";
    assert(xiaozhi_load_or_create_client_id(client_id, sizeof(client_id)));
    assert(strcmp(client_id, g_strings["client_id"].c_str()) == 0);
    assert(g_operations.empty() && g_close_calls == 1);

    reset_store();
    g_strings["client_id"] = "invalid";
    assert(xiaozhi_load_or_create_client_id(client_id, sizeof(client_id)));
    assert(strcmp(client_id, "00010203-0405-4607-8809-0a0b0c0d0e0f") == 0);
    assert((g_operations == std::vector<std::string>{"str:client_id", "commit"}));

    reset_store();
    g_strings["ws_url"] = "wss://example.test/ws";
    assert(xiaozhi_clear_activation_storage());
    assert(g_strings.empty());
    assert((g_operations == std::vector<std::string>{"erase_all", "commit"}));
    assert(g_close_calls == 1);

    reset_store();
    g_erase_error = ESP_FAIL;
    assert(!xiaozhi_clear_activation_storage());
    assert((g_operations == std::vector<std::string>{"erase_all"}));
    assert(g_close_calls == 1);

    reset_store();
    g_open_error = ESP_FAIL;
    assert(!xiaozhi_clear_activation_storage());
    assert(g_operations.empty() && g_close_calls == 0);
    return 0;
}
