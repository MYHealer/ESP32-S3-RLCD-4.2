// 验证 NVS 作用域句柄在析构、显式关闭和保存失败路径中只关闭一次。
#include "network_config_nvs.h"

#include <assert.h>

namespace {
int g_close_calls = 0;
nvs_handle_t g_last_closed_handle = 0;
esp_err_t g_open_result = ESP_OK;
nvs_handle_t g_next_open_handle = 0;
}

namespace network_config_nvs {
esp_err_t open_wifi_nvs(nvs_open_mode_t,
                        nvs_handle_t *handle,
                        const char *,
                        bool)
{
    if (g_open_result == ESP_OK && handle) {
        *handle = g_next_open_handle;
    }
    return g_open_result;
}
} // namespace network_config_nvs

void nvs_close(nvs_handle_t handle)
{
    ++g_close_calls;
    g_last_closed_handle = handle;
}

int main()
{
    {
        g_next_open_handle = 11;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_OK);
        assert(handle.get() == 11);
    }
    assert(g_close_calls == 1);
    assert(g_last_closed_handle == 11);

    {
        g_next_open_handle = 22;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_OK);
        handle.close();
        handle.close();
    }
    assert(g_close_calls == 2);
    assert(g_last_closed_handle == 22);

    {
        g_next_open_handle = 33;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_OK);
        assert(handle.close_save_ok(ESP_OK));
    }
    assert(g_close_calls == 3);
    assert(g_last_closed_handle == 33);

    {
        g_next_open_handle = 44;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_OK);
        assert(!handle.close_save_ok(ESP_FAIL));
    }
    assert(g_close_calls == 4);
    assert(g_last_closed_handle == 44);

    {
        g_open_result = ESP_FAIL;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_FAIL);
    }
    assert(g_close_calls == 4);

    {
        g_open_result = ESP_OK;
        g_next_open_handle = 55;
        network_config_nvs::ScopedNvsHandle handle;
        assert(handle.open(NVS_READWRITE, "test") == ESP_OK);
        g_next_open_handle = 66;
        assert(handle.open(NVS_READONLY, "test") == ESP_OK);
        assert(g_close_calls == 5);
        assert(g_last_closed_handle == 55);
    }
    assert(g_close_calls == 6);
    assert(g_last_closed_handle == 66);
    return 0;
}
