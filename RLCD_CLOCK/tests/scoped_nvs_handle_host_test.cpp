// 验证通用 NVS 作用域句柄的打开、接管、重开和失败关闭语义。
#include "scoped_nvs_handle.h"

#include <assert.h>
#include <type_traits>

namespace {
esp_err_t g_open_result = ESP_OK;
nvs_handle_t g_next_handle = 0;
int g_open_calls = 0;
int g_close_calls = 0;
nvs_handle_t g_last_closed_handle = 0;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t, nvs_handle_t *out)
{
    assert(name && name[0] != '\0');
    ++g_open_calls;
    if (g_open_result == ESP_OK) {
        assert(out);
        *out = g_next_handle;
    }
    return g_open_result;
}

void nvs_close(nvs_handle_t handle)
{
    ++g_close_calls;
    g_last_closed_handle = handle;
}

int main()
{
    using app_storage::ScopedNvsHandle;
    static_assert(!std::is_copy_constructible<ScopedNvsHandle>::value);
    static_assert(!std::is_copy_assignable<ScopedNvsHandle>::value);

    {
        g_next_handle = 11;
        ScopedNvsHandle handle;
        assert(handle.open("test", NVS_READONLY) == ESP_OK);
        assert(handle.get() == 11);
    }
    assert(g_open_calls == 1 && g_close_calls == 1);
    assert(g_last_closed_handle == 11);

    {
        g_next_handle = 22;
        ScopedNvsHandle handle;
        assert(handle.open("test", NVS_READWRITE) == ESP_OK);
        handle.close();
        handle.close();
    }
    assert(g_close_calls == 2 && g_last_closed_handle == 22);

    {
        g_next_handle = 33;
        ScopedNvsHandle handle;
        assert(handle.open("test", NVS_READWRITE) == ESP_OK);
        g_open_result = ESP_FAIL;
        assert(handle.open("test", NVS_READONLY) == ESP_FAIL);
        assert(g_last_closed_handle == 33);
    }
    assert(g_close_calls == 3);

    {
        g_open_result = ESP_OK;
        ScopedNvsHandle handle;
        handle.adopt(44);
        handle.adopt(55);
        assert(g_last_closed_handle == 44);
        assert(handle.get() == 55);
    }
    assert(g_close_calls == 5 && g_last_closed_handle == 55);
    return 0;
}
