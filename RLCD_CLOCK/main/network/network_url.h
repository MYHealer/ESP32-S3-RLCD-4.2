// 声明网络请求共用的 URL component 编码接口。
#pragma once

#include <stddef.h>

bool url_is_unreserved(char ch);
bool url_encode_component(const char *in, char *out, size_t out_len);
