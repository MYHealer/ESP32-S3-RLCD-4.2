// 将强制门户 DNS 查询编码为指向设备 AP 地址的 A 记录响应。
#include "captive_dns_packet.h"

#include <string.h>

namespace {
constexpr int kDnsHeaderSize = 12;
constexpr int kCaptiveDnsAnswerSize = 16;
constexpr int kDnsNamePointerSize = 2;
constexpr int kDnsTypeFieldSize = 2;
constexpr int kDnsClassFieldSize = 2;
constexpr int kDnsTtlFieldSize = 4;
constexpr int kDnsRdLengthFieldSize = 2;
constexpr int kDnsFlagsOffset = 2;
constexpr int kDnsQuestionCountOffset = 4;
constexpr int kDnsAnswerCountOffset = 6;
constexpr int kDnsAuthorityCountOffset = 8;
constexpr int kDnsAdditionalCountOffset = 10;
constexpr uint8_t kDnsLabelPointerMask = 0xC0;
constexpr uint16_t kDnsResponseFlagsStandardNoError = 0x8180;
constexpr uint16_t kDnsNameCompressionPointerToQuestion = 0xC00C;
constexpr uint16_t kDnsTypeARecord = 1;
constexpr uint16_t kDnsClassInternet = 1;
constexpr uint16_t kDnsIpv4AddressLength = 4;
constexpr uint8_t kDnsByteMask = 0xFF;
constexpr int kDnsByteShift8 = 8;
constexpr int kDnsByteShift16 = 16;
constexpr int kDnsByteShift24 = 24;
constexpr uint32_t kCaptiveDnsTtlSeconds = 60;
constexpr uint8_t kCaptiveDnsApIpOctets[kDnsIpv4AddressLength] = {192, 168, 4, 1};

void dns_write_u16(uint8_t *buf, int offset, uint16_t value)
{
    buf[offset] = static_cast<uint8_t>(value >> kDnsByteShift8);
    buf[offset + 1] = static_cast<uint8_t>(value & kDnsByteMask);
}

void dns_write_u32(uint8_t *buf, int offset, uint32_t value)
{
    buf[offset] = static_cast<uint8_t>(value >> kDnsByteShift24);
    buf[offset + 1] = static_cast<uint8_t>((value >> kDnsByteShift16) & kDnsByteMask);
    buf[offset + 2] = static_cast<uint8_t>((value >> kDnsByteShift8) & kDnsByteMask);
    buf[offset + 3] = static_cast<uint8_t>(value & kDnsByteMask);
}

static_assert(kDnsHeaderSize > 0, "DNS header size must be positive");
static_assert(kCaptiveDnsAnswerSize > 0, "captive DNS answer size must be positive");
static_assert(kCaptiveDnsAnswerSize ==
                  kDnsNamePointerSize + kDnsTypeFieldSize + kDnsClassFieldSize +
                      kDnsTtlFieldSize + kDnsRdLengthFieldSize + kDnsIpv4AddressLength,
              "captive DNS answer size must match the encoded A-record layout");
static_assert(kCaptiveDnsPacketSize >= kDnsHeaderSize + kCaptiveDnsAnswerSize,
              "captive DNS packet must fit a header and answer");
static_assert(kDnsFlagsOffset + 1 < kDnsHeaderSize, "DNS flags offset must fit header");
static_assert(kDnsQuestionCountOffset + 1 < kDnsHeaderSize, "DNS question count offset must fit header");
static_assert(kDnsAnswerCountOffset + 1 < kDnsHeaderSize, "DNS answer count offset must fit header");
static_assert(kDnsAuthorityCountOffset + 1 < kDnsHeaderSize, "DNS authority count offset must fit header");
static_assert(kDnsAdditionalCountOffset + 1 < kDnsHeaderSize, "DNS additional count offset must fit header");
static_assert(kDnsIpv4AddressLength == 4, "DNS IPv4 address length must stay four bytes");
static_assert(sizeof(kCaptiveDnsApIpOctets) == kDnsIpv4AddressLength,
              "captive DNS AP IP octet table must match IPv4 length");
static_assert(kDnsByteMask == 0xFF, "DNS byte mask must keep one byte");
static_assert(kDnsByteShift8 == 8 && kDnsByteShift16 == 16 && kDnsByteShift24 == 24,
              "DNS byte shifts must match network byte order packing");
static_assert(kCaptiveDnsTtlSeconds > 0, "captive DNS TTL must be positive");
} // namespace

int build_captive_dns_response(const uint8_t *query, int query_len, uint8_t *response, int response_len)
{
    if (!query || !response || query_len < kDnsHeaderSize ||
        response_len < kCaptiveDnsAnswerSize ||
        query_len > response_len - kCaptiveDnsAnswerSize) {
        return 0;
    }
    uint16_t qd_count = (static_cast<uint16_t>(query[kDnsQuestionCountOffset]) << 8) |
                        query[kDnsQuestionCountOffset + 1];
    if (qd_count == 0) {
        return 0;
    }

    int pos = kDnsHeaderSize;
    while (pos < query_len) {
        uint8_t label_len = query[pos++];
        if (label_len == 0) {
            break;
        }
        if ((label_len & kDnsLabelPointerMask) != 0 || pos + label_len > query_len) {
            return 0;
        }
        pos += label_len;
    }
    if (pos + 4 > query_len) {
        return 0;
    }
    int question_len = pos + 4;
    int answer_len = question_len + kCaptiveDnsAnswerSize;
    if (answer_len > response_len) {
        return 0;
    }

    if (response != query) {
        memcpy(response, query, question_len);
    }
    dns_write_u16(response, kDnsFlagsOffset, kDnsResponseFlagsStandardNoError);
    dns_write_u16(response, kDnsQuestionCountOffset, 1);
    dns_write_u16(response, kDnsAnswerCountOffset, 1);
    dns_write_u16(response, kDnsAuthorityCountOffset, 0);
    dns_write_u16(response, kDnsAdditionalCountOffset, 0);

    int out = question_len;
    dns_write_u16(response, out, kDnsNameCompressionPointerToQuestion);
    out += kDnsNamePointerSize;
    dns_write_u16(response, out, kDnsTypeARecord);
    out += kDnsTypeFieldSize;
    dns_write_u16(response, out, kDnsClassInternet);
    out += kDnsClassFieldSize;
    dns_write_u32(response, out, kCaptiveDnsTtlSeconds);
    out += kDnsTtlFieldSize;
    dns_write_u16(response, out, kDnsIpv4AddressLength);
    out += kDnsRdLengthFieldSize;
    for (uint8_t octet : kCaptiveDnsApIpOctets) {
        response[out++] = octet;
    }
    return out;
}
