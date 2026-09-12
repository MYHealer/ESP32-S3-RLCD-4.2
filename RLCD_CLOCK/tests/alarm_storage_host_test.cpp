// 验证闹钟 NVS 三键记录的读取分类、逐键条件写入、顺序提交和清除错误语义。
#include "alarm_storage.h"

#include <assert.h>
#include <map>
#include <string>
#include <vector>

#include "nvs.h"

namespace {
constexpr nvs_handle_t kHandle = 7;
std::map<std::string, uint8_t> g_values;
std::vector<std::string> g_set_keys;
esp_err_t g_open_error = ESP_OK;
int g_fail_set_call = -1;
esp_err_t g_erase_error = ESP_OK;
esp_err_t g_commit_error = ESP_OK;
int g_close_calls = 0;
int g_commit_calls = 0;
int g_erase_calls = 0;

void reset_store()
{
    g_values.clear();
    g_set_keys.clear();
    g_open_error = ESP_OK;
    g_fail_set_call = -1;
    g_erase_error = ESP_OK;
    g_commit_error = ESP_OK;
    g_close_calls = 0;
    g_commit_calls = 0;
    g_erase_calls = 0;
}

void populate(uint8_t enabled, uint8_t hour, uint8_t minute)
{
    g_values["enabled"] = enabled;
    g_values["hour"] = hour;
    g_values["minute"] = minute;
}
} // namespace

esp_err_t nvs_open(const char *name, nvs_open_mode_t, nvs_handle_t *out)
{
    assert(std::string(name) == "alarm_v1");
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

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out)
{
    assert(handle == kHandle);
    auto found = g_values.find(key);
    if (found == g_values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out = found->second;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    assert(handle == kHandle);
    g_set_keys.emplace_back(key);
    if (g_fail_set_call == static_cast<int>(g_set_keys.size())) {
        return ESP_FAIL;
    }
    g_values[key] = value;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == kHandle);
    ++g_commit_calls;
    return g_commit_error;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    assert(handle == kHandle);
    ++g_erase_calls;
    if (g_erase_error == ESP_OK) {
        g_values.clear();
    }
    return g_erase_error;
}

int main()
{
    reset_store();
    g_open_error = ESP_ERR_NVS_NOT_FOUND;
    alarm_storage::ReadResult read = alarm_storage::read();
    assert(read.status == alarm_storage::ReadStatus::kEmpty);
    assert(g_close_calls == 0);

    reset_store();
    read = alarm_storage::read();
    assert(read.status == alarm_storage::ReadStatus::kEmpty);
    assert(g_close_calls == 1);

    reset_store();
    g_values["enabled"] = 1;
    read = alarm_storage::read();
    assert(read.status == alarm_storage::ReadStatus::kIncomplete);
    assert(read.error == ESP_ERR_NVS_NOT_FOUND);

    reset_store();
    populate(1, 6, 30);
    read = alarm_storage::read();
    assert(read.status == alarm_storage::ReadStatus::kLoaded);
    assert(read.enabled == 1 && read.hour == 6 && read.minute == 30);

    reset_store();
    g_open_error = ESP_FAIL;
    read = alarm_storage::read();
    assert(read.status == alarm_storage::ReadStatus::kOpenFailed);
    assert(read.error == ESP_FAIL);

    reset_store();
    alarm_storage::WriteResult write = alarm_storage::write(true, 7, 45);
    assert(write.status == alarm_storage::WriteStatus::kSaved);
    assert((g_set_keys == std::vector<std::string>{"enabled", "hour", "minute"}));
    assert(g_values["enabled"] == 1 && g_values["hour"] == 7 && g_values["minute"] == 45);
    assert(g_commit_calls == 1 && g_close_calls == 1);

    reset_store();
    populate(1, 7, 45);
    write = alarm_storage::write(true, 7, 45);
    assert(write.status == alarm_storage::WriteStatus::kSaved);
    assert(g_set_keys.empty());
    assert(g_commit_calls == 0 && g_close_calls == 1);

    reset_store();
    populate(1, 7, 45);
    write = alarm_storage::write(false, 7, 45);
    assert(write.status == alarm_storage::WriteStatus::kSaved);
    assert((g_set_keys == std::vector<std::string>{"enabled"}));
    assert(g_commit_calls == 1 && g_close_calls == 1);

    reset_store();
    g_values["enabled"] = 1;
    write = alarm_storage::write(true, 7, 45);
    assert(write.status == alarm_storage::WriteStatus::kSaved);
    assert((g_set_keys == std::vector<std::string>{"hour", "minute"}));
    assert(g_commit_calls == 1 && g_close_calls == 1);

    reset_store();
    g_fail_set_call = 2;
    write = alarm_storage::write(false, 8, 15);
    assert(write.status == alarm_storage::WriteStatus::kWriteFailed);
    assert(write.error == ESP_FAIL);
    assert((g_set_keys == std::vector<std::string>{"enabled", "hour"}));
    assert(g_commit_calls == 0 && g_close_calls == 1);

    reset_store();
    g_open_error = ESP_FAIL;
    write = alarm_storage::write(false, 0, 0);
    assert(write.status == alarm_storage::WriteStatus::kOpenFailed);
    assert(write.error == ESP_FAIL && g_close_calls == 0);

    reset_store();
    g_open_error = ESP_ERR_NVS_NOT_FOUND;
    alarm_storage::ClearResult clear = alarm_storage::clear();
    assert(clear.status == alarm_storage::ClearStatus::kAlreadyEmpty);
    assert(g_erase_calls == 0 && g_commit_calls == 0 && g_close_calls == 0);

    reset_store();
    populate(1, 9, 5);
    clear = alarm_storage::clear();
    assert(clear.status == alarm_storage::ClearStatus::kCleared);
    assert(g_values.empty());
    assert(g_erase_calls == 1 && g_commit_calls == 1 && g_close_calls == 1);

    reset_store();
    populate(1, 9, 5);
    g_erase_error = ESP_FAIL;
    clear = alarm_storage::clear();
    assert(clear.status == alarm_storage::ClearStatus::kEraseFailed);
    assert(clear.error == ESP_FAIL);
    assert(g_commit_calls == 0 && g_close_calls == 1);
    return 0;
}
