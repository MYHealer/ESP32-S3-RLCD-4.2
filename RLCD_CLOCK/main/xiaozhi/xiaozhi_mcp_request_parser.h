// 解析小智 MCP 请求信封，并提供只读字段视图。
#pragma once

#include "xiaozhi_json_owner.h"

#include <stddef.h>
#include <string.h>

inline bool xiaozhi_mcp_json_token_present(const char *message, size_t message_len)
{
    constexpr char kMcpJsonToken[] = "\"mcp\"";
    constexpr size_t kMcpJsonTokenLen = sizeof(kMcpJsonToken) - 1;
    if (!message || message_len < kMcpJsonTokenLen) {
        return false;
    }
    for (size_t offset = 0; offset <= message_len - kMcpJsonTokenLen; ++offset) {
        if (memcmp(message + offset, kMcpJsonToken, kMcpJsonTokenLen) == 0) {
            return true;
        }
    }
    return false;
}

class XiaozhiMcpRequestDocument {
public:
    XiaozhiMcpRequestDocument() = default;

    XiaozhiMcpRequestDocument(const XiaozhiMcpRequestDocument &) = delete;
    XiaozhiMcpRequestDocument &operator=(const XiaozhiMcpRequestDocument &) = delete;

    bool parse(const char *message, size_t message_len)
    {
        type_ = nullptr;
        version_ = nullptr;
        method_ = nullptr;
        id_ = nullptr;
        params_ = nullptr;
        tool_name_ = nullptr;
        root_.reset(cJSON_ParseWithLength(message, message_len));
        if (!root_) {
            return false;
        }

        type_ = json_string_value(cJSON_GetObjectItem(root_.get(), "type"));
        const cJSON *payload = cJSON_GetObjectItem(root_.get(), "payload");
        if (cJSON_IsObject(payload)) {
            version_ = json_string_value(cJSON_GetObjectItem(payload, "jsonrpc"));
            method_ = json_string_value(cJSON_GetObjectItem(payload, "method"));
            id_ = cJSON_GetObjectItem(payload, "id");
            params_ = cJSON_GetObjectItem(payload, "params");
        }
        if (cJSON_IsObject(params_)) {
            tool_name_ = json_string_value(cJSON_GetObjectItem(params_, "name"));
        }
        return true;
    }

    const char *type() const
    {
        return type_;
    }

    const char *version() const
    {
        return version_;
    }

    const char *method() const
    {
        return method_;
    }

    const cJSON *id() const
    {
        return id_;
    }

    const cJSON *params() const
    {
        return params_;
    }

    const char *tool_name() const
    {
        return tool_name_;
    }

private:
    static const char *json_string_value(const cJSON *item)
    {
        return cJSON_IsString(item) ? item->valuestring : nullptr;
    }

    XiaozhiJsonOwner root_;
    const char *type_ = nullptr;
    const char *version_ = nullptr;
    const char *method_ = nullptr;
    const cJSON *id_ = nullptr;
    const cJSON *params_ = nullptr;
    const char *tool_name_ = nullptr;
};
