// 声明小智设备标识格式化和官方激活 HTTP 请求入口。
#pragma once

#include <stddef.h>

inline constexpr size_t kXiaozhiActivationResponseSize = 3072;
inline constexpr size_t kXiaozhiActivationRequestSize = 1536;
inline constexpr size_t kXiaozhiDeviceIdSize = 18;

struct XiaozhiActivationResponse {
    char data[kXiaozhiActivationResponseSize] = {};
    size_t len = 0;
};

struct XiaozhiActivationScratch {
    char request[kXiaozhiActivationRequestSize] = {};
    XiaozhiActivationResponse response;
};

inline void xiaozhi_reset_activation_response(XiaozhiActivationResponse *response)
{
    if (!response) {
        return;
    }
    response->data[0] = '\0';
    response->len = 0;
}

inline size_t xiaozhi_activation_response_writable_bytes(
    const XiaozhiActivationResponse *response)
{
    if (!response || response->len >= sizeof(response->data) - 1) {
        return 0;
    }
    return sizeof(response->data) - response->len - 1;
}

void xiaozhi_format_device_id(char *out, size_t out_len);
bool xiaozhi_request_activation(XiaozhiActivationScratch *scratch);
