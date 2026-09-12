// 提供跨业务复用的 NVS 句柄作用域所有权，避免错误出口遗漏关闭。
#pragma once

#include "esp_err.h"
#include "nvs.h"

namespace app_storage {

class ScopedNvsHandle {
public:
    ScopedNvsHandle() = default;
    ~ScopedNvsHandle() { close(); }

    ScopedNvsHandle(const ScopedNvsHandle &) = delete;
    ScopedNvsHandle &operator=(const ScopedNvsHandle &) = delete;

    esp_err_t open(const char *name, nvs_open_mode_t mode)
    {
        close();
        nvs_handle_t handle = 0;
        esp_err_t err = nvs_open(name, mode, &handle);
        if (err == ESP_OK) {
            adopt(handle);
        }
        return err;
    }

    void adopt(nvs_handle_t handle)
    {
        close();
        handle_ = handle;
        open_ = true;
    }

    nvs_handle_t get() const { return handle_; }

    void close()
    {
        if (!open_) {
            return;
        }
        nvs_close(handle_);
        handle_ = 0;
        open_ = false;
    }

private:
    nvs_handle_t handle_ = 0;
    bool open_ = false;
};

} // namespace app_storage
