// 验证整点提醒四项 NVS 的默认读取、完整匹配、条件写入和错误短路。
#include "network_chime_storage.h"

#include <assert.h>
#include <map>
#include <string>

namespace {
std::map<std::string, uint8_t> g_values;
int g_get_calls = 0;
int g_set_calls = 0;
int g_fail_set_call = -1;

void reset_store()
{
    g_values.clear();
    g_get_calls = 0;
    g_set_calls = 0;
    g_fail_set_call = -1;
}
} // namespace

esp_err_t nvs_get_u8(nvs_handle_t, const char *key, uint8_t *value)
{
    ++g_get_calls;
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    auto found = g_values.find(key);
    if (found == g_values.end()) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *value = found->second;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t, const char *key, uint8_t value)
{
    ++g_set_calls;
    if (g_fail_set_call == g_set_calls) {
        return ESP_FAIL;
    }
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    g_values[key] = value;
    return ESP_OK;
}

int main()
{
    reset_store();
    network_chime_storage::StoredChimeSettings loaded = network_chime_storage::read(1, 80);
    assert(loaded.enabled == 0);
    assert(loaded.all_day == 0);
    assert(loaded.volume == 80);
    assert(loaded.sound == 0);
    assert(g_get_calls == 4);

    network_chime_storage::StoredChimeSettings expected = {1, 1, 60, 2};
    bool changed = false;
    assert(network_chime_storage::write_if_changed(1, ESP_OK, expected, &changed) == ESP_OK);
    assert(changed);
    assert(g_set_calls == 4);
    assert(network_chime_storage::matches(1, expected));

    g_get_calls = 0;
    g_set_calls = 0;
    changed = true;
    assert(network_chime_storage::write_if_changed(1, ESP_OK, expected, &changed) == ESP_OK);
    assert(!changed);
    assert(g_get_calls == 4);
    assert(g_set_calls == 0);

    g_values[network_chime_storage::kChimeVolumeKey] = 40;
    g_get_calls = 0;
    g_set_calls = 0;
    assert(network_chime_storage::write_if_changed(1, ESP_OK, expected, &changed) == ESP_OK);
    assert(changed);
    assert(g_get_calls == 4);
    assert(g_set_calls == 1);
    assert(network_chime_storage::matches(1, expected));

    g_get_calls = 0;
    g_set_calls = 0;
    changed = true;
    assert(network_chime_storage::write_if_changed(1, ESP_FAIL, expected, &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_get_calls == 0);
    assert(g_set_calls == 0);

    g_values[network_chime_storage::kHourlyChimeKey] = 0;
    g_values.erase(network_chime_storage::kChimeSoundKey);
    g_get_calls = 0;
    g_set_calls = 0;
    g_fail_set_call = 2;
    assert(network_chime_storage::write_if_changed(1, ESP_OK, expected, &changed) == ESP_FAIL);
    assert(!changed);
    assert(g_set_calls == 2);
    return 0;
}
