// 管理 QWeather 单次请求 URL、响应、解析暂存内存和业务成功字段读取。
#include "qweather_response.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "network_json.h"

#include <esp_log.h>
#include <stdint.h>
#include <string.h>

namespace {
constexpr const char *kQweatherDefaultStage = "request";
constexpr const char *kQweatherJsonCodeField = "code";
constexpr const char *kQweatherJsonMetadataField = "metadata";
constexpr const char *kQweatherJsonZeroResultField = "zeroResult";
constexpr const char *kQweatherSuccessCode = "200";
constexpr const char *kQweatherMissingCodeText = "missing";
#define QWEATHER_RESPONSE_SIZE_INVALID_FORMAT "qweather %s response size invalid"
#define QWEATHER_RESPONSE_ALLOC_FAILED_FORMAT "qweather %s response alloc failed"

size_t qweather_exchange_buffer_size(size_t response_size,
                                     size_t request_url_size,
                                     size_t staging_size)
{
    if (response_size == 0 || request_url_size > SIZE_MAX - response_size) {
        return 0;
    }
    const size_t exchange_size = request_url_size + response_size;
    return staging_size <= SIZE_MAX - exchange_size
               ? exchange_size + staging_size
               : 0;
}
} // namespace

const char *qweather_stage_text(const char *stage)
{
    return cstr_nonempty(stage) ? stage : kQweatherDefaultStage;
}

QweatherResponseBuffer::QweatherResponseBuffer(const char *stage, size_t buffer_size)
    : QweatherResponseBuffer(stage, buffer_size, 0, 0)
{
}

QweatherResponseBuffer::QweatherResponseBuffer(const char *stage,
                                               size_t buffer_size,
                                               size_t request_url_size)
    : QweatherResponseBuffer(stage, buffer_size, request_url_size, 0)
{
}

QweatherResponseBuffer::QweatherResponseBuffer(const char *stage,
                                               size_t buffer_size,
                                               size_t request_url_size,
                                               size_t staging_size)
    : data_(qweather_exchange_buffer_size(buffer_size,
                                         request_url_size,
                                         staging_size),
            HeapBufferInit::kUninitialized,
            HeapBufferStorage::kPsramRequired),
      size_(buffer_size),
      request_url_size_(request_url_size),
      staging_size_(staging_size)
{
    if (qweather_exchange_buffer_size(buffer_size,
                                     request_url_size,
                                     staging_size) == 0) {
        ESP_LOGW(TAG, QWEATHER_RESPONSE_SIZE_INVALID_FORMAT, qweather_stage_text(stage));
    } else if (!data_) {
        ESP_LOGW(TAG, QWEATHER_RESPONSE_ALLOC_FAILED_FORMAT, qweather_stage_text(stage));
    } else {
        data_.get()[0] = '\0';
        get()[0] = '\0';
    }
}

QweatherResponseBuffer::~QweatherResponseBuffer() = default;

QweatherJsonRoot::QweatherJsonRoot(char *response)
    : root_(response)
{
}

QweatherJsonRoot::~QweatherJsonRoot() = default;

const char *qweather_json_string_value(const cJSON *item)
{
    return network_json_string_value(item);
}

bool qweather_code_ok(const cJSON *code)
{
    const char *text = qweather_json_string_value(code);
    return text && strcmp(text, kQweatherSuccessCode) == 0;
}

const char *qweather_code_text(const cJSON *code)
{
    const char *text = qweather_json_string_value(code);
    return text ? text : kQweatherMissingCodeText;
}

const cJSON *qweather_success_item(const cJSON *root, const char *field, const cJSON **code_out)
{
    const cJSON *code = root ? cJSON_GetObjectItem(root, kQweatherJsonCodeField) : nullptr;
    if (code_out) {
        *code_out = code;
    }
    const cJSON *item = root && field ? cJSON_GetObjectItem(root, field) : nullptr;
    return qweather_code_ok(code) ? item : nullptr;
}

const cJSON *qweather_success_object(const cJSON *root, const char *field, const cJSON **code_out)
{
    const cJSON *item = qweather_success_item(root, field, code_out);
    return cJSON_IsObject(item) ? item : nullptr;
}

const cJSON *qweather_success_array(const cJSON *root, const char *field, const cJSON **code_out)
{
    const cJSON *item = qweather_success_item(root, field, code_out);
    return cJSON_IsArray(item) ? item : nullptr;
}

const cJSON *qweather_alert_success_array(const cJSON *root,
                                         const char *field,
                                         const cJSON **code_out)
{
    const cJSON *code =
        cJSON_IsObject(root) ? cJSON_GetObjectItem(root, kQweatherJsonCodeField) : nullptr;
    if (code_out) {
        *code_out = code;
    }
    if (code) {
        return qweather_success_array(root, field, nullptr);
    }
    const cJSON *metadata =
        cJSON_IsObject(root) ? cJSON_GetObjectItem(root, kQweatherJsonMetadataField) : nullptr;
    const cJSON *zero_result =
        cJSON_IsObject(metadata)
            ? cJSON_GetObjectItem(metadata, kQweatherJsonZeroResultField)
            : nullptr;
    const cJSON *alerts =
        cJSON_IsObject(root) && field ? cJSON_GetObjectItem(root, field) : nullptr;
    return cJSON_IsBool(zero_result) && cJSON_IsArray(alerts)
               ? alerts
               : nullptr;
}
