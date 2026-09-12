// 声明网络检测公网 IPv4 响应的纯解析接口。
#pragma once

#include <stddef.h>

bool network_public_ip_parse_response(const char *response,
                                      char *out,
                                      size_t out_len);
