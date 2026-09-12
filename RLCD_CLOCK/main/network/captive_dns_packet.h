// 声明强制门户 DNS 报文编码接口和固定报文容量。
#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr int kCaptiveDnsPacketSize = 512;

int build_captive_dns_response(const uint8_t *query,
                               int query_len,
                               uint8_t *response,
                               int response_len);
