// 解析小智激活响应中的绑定提示、绑定码、挑战和 WebSocket 配置节点。
#pragma once

#include "xiaozhi_json_owner.h"

#include <stddef.h>

class XiaozhiActivationResponseDocument {
public:
    XiaozhiActivationResponseDocument() = default;

    XiaozhiActivationResponseDocument(const XiaozhiActivationResponseDocument &) = delete;
    XiaozhiActivationResponseDocument &operator=(const XiaozhiActivationResponseDocument &) = delete;

    bool parse(const char *json, size_t json_len)
    {
        message_ = nullptr;
        binding_code_ = nullptr;
        challenge_ = nullptr;
        websocket_ = nullptr;
        root_.reset(cJSON_ParseWithLength(json, json_len));
        if (!root_) {
            return false;
        }

        cJSON *activation = cJSON_GetObjectItem(root_.get(), "activation");
        websocket_ = cJSON_GetObjectItem(root_.get(), "websocket");
        if (cJSON_IsObject(activation)) {
            message_ = json_string_value(cJSON_GetObjectItem(activation, "message"));
            binding_code_ = json_string_value(cJSON_GetObjectItem(activation, "code"));
            challenge_ = json_string_value(cJSON_GetObjectItem(activation, "challenge"));
        }
        return true;
    }

    const char *message() const
    {
        return message_;
    }

    const char *binding_code() const
    {
        return binding_code_;
    }

    const char *challenge() const
    {
        return challenge_;
    }

    cJSON *websocket() const
    {
        return websocket_;
    }

private:
    static const char *json_string_value(const cJSON *item)
    {
        return cJSON_IsString(item) ? item->valuestring : nullptr;
    }

    XiaozhiJsonOwner root_;
    const char *message_ = nullptr;
    const char *binding_code_ = nullptr;
    const char *challenge_ = nullptr;
    cJSON *websocket_ = nullptr;
};
