// 定义普通 HTTP 失败是否适合在同一联网窗口立即重试。
#pragma once

#include <esp_err.h>

constexpr bool network_http_immediate_retry_allowed(esp_err_t err)
{
    if (err == ESP_OK) {
        return false;
    }
    switch (err) {
    case ESP_ERR_NO_MEM:
    case ESP_ERR_INVALID_ARG:
    case ESP_ERR_INVALID_STATE:
    case ESP_ERR_INVALID_SIZE:
    case ESP_ERR_NOT_SUPPORTED:
        return false;
    default:
        return true;
    }
}

