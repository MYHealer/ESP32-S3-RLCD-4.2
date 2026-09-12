// 声明 HTTP gzip 响应的魔数和 payload 范围解析接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace network_gzip_detail {
inline constexpr uint8_t kMagic0 = 0x1F;
inline constexpr uint8_t kMagic1 = 0x8B;
inline constexpr size_t kMagicPrefixSize = 2;

inline bool has_magic_prefix(const char *data, size_t len)
{
    return data && len >= kMagicPrefixSize &&
           static_cast<uint8_t>(data[0]) == kMagic0 &&
           static_cast<uint8_t>(data[1]) == kMagic1;
}
} // namespace network_gzip_detail

bool gzip_payload_range(const uint8_t *data,
                        size_t len,
                        size_t *payload_offset,
                        size_t *payload_len);
