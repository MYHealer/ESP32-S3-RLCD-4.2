// 声明 OTA 固件下载 HTTP 会话、重定向和幂等清理所有权。
#pragma once

#include "scoped_heap_buffer.h"

#include <esp_http_client.h>

#include <cstddef>

class OtaDownloadHttpSession {
public:
    OtaDownloadHttpSession();
    ~OtaDownloadHttpSession();

    OtaDownloadHttpSession(const OtaDownloadHttpSession &) = delete;
    OtaDownloadHttpSession &operator=(const OtaDownloadHttpSession &) = delete;

    bool open(const char *url);
    void close();

    esp_http_client_handle_t handle() const
    {
        return client_;
    }

    int content_length() const
    {
        return content_length_;
    }

private:
    static esp_err_t event_handler(esp_http_client_event_t *event);

    static constexpr size_t kRedirectUrlCapacity = 1024;
    esp_http_client_handle_t client_ = nullptr;
    bool opened_ = false;
    bool redirect_location_seen_ = false;
    bool redirect_location_invalid_ = false;
    int content_length_ = 0;
    ScopedHeapBuffer<char> redirect_url_;
};
