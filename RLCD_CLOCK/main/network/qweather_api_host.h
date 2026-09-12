// 声明 QWeather 专属 API Host 的长度、规范化和校验接口。
#pragma once

#include <stddef.h>

inline constexpr size_t kQweatherApiHostLen = 128;

bool normalize_qweather_api_host(const char *input,
                                 char *out,
                                 size_t out_len);
bool qweather_api_host_input_valid(const char *input);
