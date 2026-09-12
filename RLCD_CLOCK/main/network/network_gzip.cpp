// 实现 HTTP gzip 响应头的边界检查和 payload 范围解析。
#include "network_gzip.h"

namespace {
constexpr uint8_t kGzipDeflateMethod = 8;
constexpr uint8_t kGzipFlagHeaderCrc = 0x02;
constexpr uint8_t kGzipFlagExtra = 0x04;
constexpr uint8_t kGzipFlagName = 0x08;
constexpr uint8_t kGzipFlagComment = 0x10;
constexpr size_t kGzipMinSize = 18;
constexpr size_t kGzipBaseHeaderSize = 10;
constexpr size_t kGzipTrailerSize = 8;
constexpr size_t kGzipExtraLengthFieldSize = 2;

constexpr bool gzip_flag_bits_valid()
{
    constexpr uint8_t flags[] = {
        kGzipFlagHeaderCrc,
        kGzipFlagExtra,
        kGzipFlagName,
        kGzipFlagComment,
    };
    uint8_t combined = 0;
    for (uint8_t flag : flags) {
        if (flag == 0 || (combined & flag) != 0) {
            return false;
        }
        combined |= flag;
    }
    return true;
}

bool advance_gzip_pos(size_t *pos, size_t amount, size_t len)
{
    if (!pos || amount > len || *pos > len - amount) {
        return false;
    }
    *pos += amount;
    return true;
}

bool skip_gzip_zero_terminated_field(const uint8_t *data, size_t len, size_t *pos)
{
    if (!data || !pos) {
        return false;
    }
    while (*pos < len && data[*pos] != 0) {
        ++(*pos);
    }
    return advance_gzip_pos(pos, 1, len);
}

static_assert(network_gzip_detail::kMagicPrefixSize == 2,
              "gzip magic prefix must contain two bytes");
static_assert(network_gzip_detail::kMagic0 == 0x1F &&
                  network_gzip_detail::kMagic1 == 0x8B,
              "gzip magic bytes must remain RFC1952 values");
static_assert(kGzipDeflateMethod == 8,
              "gzip compression method must remain deflate");
static_assert(kGzipBaseHeaderSize > network_gzip_detail::kMagicPrefixSize,
              "gzip base header must include fields after magic");
static_assert(kGzipMinSize >= kGzipBaseHeaderSize + kGzipTrailerSize,
              "gzip minimum size must cover base header and trailer");
static_assert(kGzipExtraLengthFieldSize == 2,
              "gzip extra length field is two bytes");
static_assert(gzip_flag_bits_valid(),
              "gzip optional header flags must be nonzero and non-overlapping");
} // namespace

bool gzip_payload_range(const uint8_t *data,
                        size_t len,
                        size_t *payload_offset,
                        size_t *payload_len)
{
    if (!data || !payload_offset || !payload_len) {
        return false;
    }
    if (len < kGzipMinSize || data[0] != network_gzip_detail::kMagic0 ||
        data[1] != network_gzip_detail::kMagic1 || data[2] != kGzipDeflateMethod) {
        return false;
    }

    uint8_t flags = data[3];
    size_t pos = kGzipBaseHeaderSize;
    if (flags & kGzipFlagExtra) {
        if (pos > len || kGzipExtraLengthFieldSize > len - pos) {
            return false;
        }
        size_t extra_len = data[pos] | (data[pos + 1] << 8);
        if (!advance_gzip_pos(&pos, kGzipExtraLengthFieldSize, len) ||
            !advance_gzip_pos(&pos, extra_len, len)) {
            return false;
        }
    }
    if (flags & kGzipFlagName) {
        if (!skip_gzip_zero_terminated_field(data, len, &pos)) {
            return false;
        }
    }
    if (flags & kGzipFlagComment) {
        if (!skip_gzip_zero_terminated_field(data, len, &pos)) {
            return false;
        }
    }
    if (flags & kGzipFlagHeaderCrc) {
        if (!advance_gzip_pos(&pos, kGzipExtraLengthFieldSize, len)) {
            return false;
        }
    }
    if (pos > len || kGzipTrailerSize > len - pos) {
        return false;
    }

    *payload_offset = pos;
    *payload_len = len - pos - kGzipTrailerSize;
    return true;
}
