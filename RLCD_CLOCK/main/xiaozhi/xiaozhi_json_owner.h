// 提供小智模块统一复用的 cJSON 根对象所有权管理。
#pragma once

#include "cJSON.h"

class XiaozhiJsonOwner {
public:
    explicit XiaozhiJsonOwner(cJSON *value = nullptr)
        : value_(value)
    {
    }

    ~XiaozhiJsonOwner()
    {
        cJSON_Delete(value_);
    }

    XiaozhiJsonOwner(const XiaozhiJsonOwner &) = delete;
    XiaozhiJsonOwner &operator=(const XiaozhiJsonOwner &) = delete;

    cJSON *get()
    {
        return value_;
    }

    const cJSON *get() const
    {
        return value_;
    }

    explicit operator bool() const
    {
        return value_ != nullptr;
    }

    void reset(cJSON *value = nullptr)
    {
        if (value_ == value) {
            return;
        }
        cJSON_Delete(value_);
        value_ = value;
    }

private:
    cJSON *value_ = nullptr;
};
