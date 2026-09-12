// 提供 esp_http_client 句柄的作用域清理守卫。
#pragma once

#include <esp_http_client.h>

class ScopedHttpClient {
public:
    explicit ScopedHttpClient(const esp_http_client_config_t *config)
        : client_(config ? esp_http_client_init(config) : nullptr)
    {
    }

    ~ScopedHttpClient()
    {
        if (client_) {
            esp_http_client_cleanup(client_);
        }
    }

    ScopedHttpClient(const ScopedHttpClient &) = delete;
    ScopedHttpClient &operator=(const ScopedHttpClient &) = delete;

    esp_http_client_handle_t get() const
    {
        return client_;
    }

    explicit operator bool() const
    {
        return client_ != nullptr;
    }

private:
    esp_http_client_handle_t client_ = nullptr;
};
