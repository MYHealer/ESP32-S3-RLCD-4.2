// 声明网络响应模块共用的只读 JSON 字符串复制接口。
#pragma once

#include "cJSON.h"

#include <stddef.h>

const char *network_json_string_value(const cJSON *item);
const char *network_json_object_string_value(const cJSON *obj,
                                             const char *name);
bool json_copy_string(const cJSON *obj,
                      const char *name,
                      char *out,
                      size_t out_len);
bool json_copy_string_exact(const cJSON *obj,
                            const char *name,
                            char *out,
                            size_t out_len);
