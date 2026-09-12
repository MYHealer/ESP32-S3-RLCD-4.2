// 声明配网页复用的 application/x-www-form-urlencoded 解析接口。
#pragma once

#include <cstddef>

inline constexpr size_t kNetworkFormEncodedBufferSize = 160;

void url_decode(char *dst, size_t dst_len, const char *src);
void form_value(const char *body, const char *key, char *out, size_t out_len);
void form_value_fallback(const char *body,
                         const char *primary_key,
                         const char *fallback_key,
                         char *out,
                         size_t out_len);
