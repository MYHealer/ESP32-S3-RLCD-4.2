// 声明 QWeather 单次请求 URL、响应、解析暂存缓冲及业务成功字段读取接口。
#pragma once

#include "cJSON.h"
#include "network_json_root.h"
#include "scoped_heap_buffer.h"

#include <stddef.h>

const char *qweather_stage_text(const char *stage);

class QweatherResponseBuffer {
public:
    QweatherResponseBuffer(const char *stage, size_t buffer_size);
    QweatherResponseBuffer(const char *stage,
                           size_t buffer_size,
                           size_t request_url_size);
    QweatherResponseBuffer(const char *stage,
                           size_t buffer_size,
                           size_t request_url_size,
                           size_t staging_size);
    ~QweatherResponseBuffer();

    QweatherResponseBuffer(const QweatherResponseBuffer &) = delete;
    QweatherResponseBuffer &operator=(const QweatherResponseBuffer &) = delete;

    char *get() const
    {
        return data_ ? data_.get() + request_url_size_ : nullptr;
    }

    size_t size() const
    {
        return size_;
    }

    char *request_url() const
    {
        return data_ && request_url_size_ > 0 ? data_.get() : nullptr;
    }

    size_t request_url_size() const
    {
        return request_url_size_;
    }

    void *staging() const
    {
        return data_ && staging_size_ > 0
                   ? data_.get() + request_url_size_ + size_
                   : nullptr;
    }

    size_t staging_size() const
    {
        return staging_size_;
    }

    explicit operator bool() const
    {
        return static_cast<bool>(data_);
    }

private:
    ScopedHeapBuffer<char> data_;
    size_t size_ = 0;
    size_t request_url_size_ = 0;
    size_t staging_size_ = 0;
};

class QweatherJsonRoot {
public:
    explicit QweatherJsonRoot(char *response);
    ~QweatherJsonRoot();

    QweatherJsonRoot(const QweatherJsonRoot &) = delete;
    QweatherJsonRoot &operator=(const QweatherJsonRoot &) = delete;

    const cJSON *get() const
    {
        return root_.get();
    }

    explicit operator bool() const
    {
        return static_cast<bool>(root_);
    }

private:
    NetworkJsonRoot root_;
};

const char *qweather_json_string_value(const cJSON *item);
bool qweather_code_ok(const cJSON *code);
const char *qweather_code_text(const cJSON *code);
const cJSON *qweather_success_item(const cJSON *root, const char *field, const cJSON **code_out);
const cJSON *qweather_success_object(const cJSON *root, const char *field, const cJSON **code_out);
const cJSON *qweather_success_array(const cJSON *root, const char *field, const cJSON **code_out);
const cJSON *qweather_alert_success_array(const cJSON *root,
                                         const char *field,
                                         const cJSON **code_out);
