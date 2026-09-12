// 声明网络文本请求、响应解码和受控日志预览接口。
#pragma once

#include "esp_err.h"
#include "esp_http_client.h"

#include <stddef.h>

struct HttpBuffer {
    char *data;
    size_t len;
    size_t cap;
    bool truncated;
};

esp_err_t http_event_handler(esp_http_client_event_t *evt);
esp_err_t decode_http_body(char *out, size_t out_len, size_t *body_len);
esp_err_t http_get_text(const char *url,
                        char *out,
                        size_t out_len,
                        const char *api_key = nullptr);
void log_response_preview(const char *stage, const char *response);
