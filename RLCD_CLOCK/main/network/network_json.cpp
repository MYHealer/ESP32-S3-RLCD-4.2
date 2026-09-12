// 实现网络响应模块共用的只读 JSON 字符串复制。
#include "network_json.h"

#include "app_text_format.h"

#include <string.h>

const char *network_json_string_value(const cJSON *item)
{
    return cJSON_IsString(item) && item->valuestring
               ? item->valuestring
               : nullptr;
}

const char *network_json_object_string_value(const cJSON *obj,
                                             const char *name)
{
    return obj && name
               ? network_json_string_value(cJSON_GetObjectItem(obj, name))
               : nullptr;
}

bool json_copy_string(const cJSON *obj,
                      const char *name,
                      char *out,
                      size_t out_len)
{
    if (!obj || !name || !app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    const char *value = network_json_object_string_value(obj, name);
    if (!value) {
        return false;
    }
    strlcpy(out, value, out_len);
    return true;
}

bool json_copy_string_exact(const cJSON *obj,
                            const char *name,
                            char *out,
                            size_t out_len)
{
    if (!obj || !name || !app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    const char *value = network_json_object_string_value(obj, name);
    if (!value) {
        return false;
    }
    const size_t value_len = strnlen(value, out_len);
    if (value_len >= out_len) {
        return false;
    }
    memcpy(out, value, value_len + 1);
    return true;
}
