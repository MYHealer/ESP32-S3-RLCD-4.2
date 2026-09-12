// 管理网络响应 JSON 根对象的解析和自动释放。
#pragma once

#include "cJSON.h"

class NetworkJsonRoot {
public:
    explicit NetworkJsonRoot(const char *response)
        : root_(response ? cJSON_Parse(response) : nullptr)
    {
    }

    ~NetworkJsonRoot()
    {
        cJSON_Delete(root_);
    }

    NetworkJsonRoot(const NetworkJsonRoot &) = delete;
    NetworkJsonRoot &operator=(const NetworkJsonRoot &) = delete;

    const cJSON *get() const
    {
        return root_;
    }

    explicit operator bool() const
    {
        return root_ != nullptr;
    }

private:
    cJSON *root_ = nullptr;
};
