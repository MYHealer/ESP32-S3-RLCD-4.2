// 验证 gzip 魔数、可选头字段和 payload 边界解析。
#include "network_gzip.h"

#include <assert.h>
#include <vector>

namespace {
constexpr uint8_t kFlagHeaderCrc = 0x02;
constexpr uint8_t kFlagExtra = 0x04;
constexpr uint8_t kFlagName = 0x08;
constexpr uint8_t kFlagComment = 0x10;

std::vector<uint8_t> gzip_bytes(uint8_t flags)
{
    std::vector<uint8_t> data = {0x1F, 0x8B, 8, flags, 0, 0, 0, 0, 0, 0};
    if (flags & kFlagExtra) {
        data.insert(data.end(), {2, 0, 0x11, 0x22});
    }
    if (flags & kFlagName) {
        data.insert(data.end(), {'n', 0});
    }
    if (flags & kFlagComment) {
        data.insert(data.end(), {'c', 0});
    }
    if (flags & kFlagHeaderCrc) {
        data.insert(data.end(), {0x33, 0x44});
    }
    data.push_back(0xAA);
    data.insert(data.end(), 8, 0);
    return data;
}

void expect_payload(uint8_t flags)
{
    std::vector<uint8_t> data = gzip_bytes(flags);
    size_t offset = 0;
    size_t len = 0;
    assert(gzip_payload_range(data.data(), data.size(), &offset, &len));
    assert(offset == data.size() - 9);
    assert(len == 1);
    assert(data[offset] == 0xAA);
}
} // namespace

int main()
{
    const char magic[] = {static_cast<char>(0x1F), static_cast<char>(0x8B), 0};
    assert(network_gzip_detail::has_magic_prefix(magic, 2));
    assert(!network_gzip_detail::has_magic_prefix(nullptr, 2));
    assert(!network_gzip_detail::has_magic_prefix(magic, 1));
    assert(!network_gzip_detail::has_magic_prefix("no", 2));

    expect_payload(0);
    expect_payload(kFlagExtra);
    expect_payload(kFlagName);
    expect_payload(kFlagComment);
    expect_payload(kFlagHeaderCrc);
    expect_payload(kFlagExtra | kFlagName | kFlagComment | kFlagHeaderCrc);

    std::vector<uint8_t> minimal = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0};
    minimal.insert(minimal.end(), 8, 0);
    size_t offset = 123;
    size_t len = 456;
    assert(gzip_payload_range(minimal.data(), minimal.size(), &offset, &len));
    assert(offset == 10 && len == 0);

    std::vector<uint8_t> bad = minimal;
    bad[0] = 0;
    offset = 123;
    len = 456;
    assert(!gzip_payload_range(bad.data(), bad.size(), &offset, &len));
    assert(offset == 123 && len == 456);
    bad = minimal;
    bad[2] = 0;
    assert(!gzip_payload_range(bad.data(), bad.size(), &offset, &len));
    assert(!gzip_payload_range(nullptr, minimal.size(), &offset, &len));
    assert(!gzip_payload_range(minimal.data(), minimal.size(), nullptr, &len));
    assert(!gzip_payload_range(minimal.data(), minimal.size(), &offset, nullptr));
    assert(!gzip_payload_range(minimal.data(), minimal.size() - 1, &offset, &len));

    bad.assign(18, 0x55);
    bad[0] = 0x1F;
    bad[1] = 0x8B;
    bad[2] = 8;
    bad[3] = kFlagName;
    assert(!gzip_payload_range(bad.data(), bad.size(), &offset, &len));

    bad = minimal;
    bad[3] = kFlagExtra;
    bad[10] = 0xFF;
    bad[11] = 0xFF;
    assert(!gzip_payload_range(bad.data(), bad.size(), &offset, &len));
    return 0;
}
