// 在主机侧验证强制门户 DNS A 记录编码和异常报文拒绝逻辑。
#include "captive_dns_packet.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

namespace {
constexpr uint8_t kExampleQuery[] = {
    0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 'e',  'x',  'a',  'm',  'p',  'l',  'e',  0x03, 'c',  'o',  'm',
    0x00, 0x00, 0x01, 0x00, 0x01,
};
constexpr uint8_t kExpectedAnswer[] = {
    0xC0, 0x0C,
    0x00, 0x01,
    0x00, 0x01,
    0x00, 0x00, 0x00, 0x3C,
    0x00, 0x04,
    192,  168,  4,    1,
};

void test_invalid_arguments()
{
    uint8_t response[kCaptiveDnsPacketSize] = {};
    assert(build_captive_dns_response(nullptr,
                                      sizeof(kExampleQuery),
                                      response,
                                      sizeof(response)) == 0);
    assert(build_captive_dns_response(kExampleQuery,
                                      sizeof(kExampleQuery),
                                      nullptr,
                                      sizeof(response)) == 0);
    assert(build_captive_dns_response(kExampleQuery, 11, response, sizeof(response)) == 0);
    assert(build_captive_dns_response(kExampleQuery,
                                      sizeof(kExampleQuery),
                                      response,
                                      sizeof(kExampleQuery) + sizeof(kExpectedAnswer) - 1) == 0);
    assert(build_captive_dns_response(kExampleQuery,
                                      INT_MAX,
                                      response,
                                      INT_MAX) == 0);
}

void test_invalid_queries()
{
    uint8_t query[sizeof(kExampleQuery)] = {};
    uint8_t response[kCaptiveDnsPacketSize] = {};

    memcpy(query, kExampleQuery, sizeof(query));
    query[4] = 0;
    query[5] = 0;
    assert(build_captive_dns_response(query, sizeof(query), response, sizeof(response)) == 0);

    memcpy(query, kExampleQuery, sizeof(query));
    query[12] = 0x20;
    assert(build_captive_dns_response(query, sizeof(query), response, sizeof(response)) == 0);

    memcpy(query, kExampleQuery, sizeof(query));
    query[12] = 0xC0;
    assert(build_captive_dns_response(query, sizeof(query), response, sizeof(response)) == 0);

    assert(build_captive_dns_response(kExampleQuery,
                                      sizeof(kExampleQuery) - 3,
                                      response,
                                      sizeof(response)) == 0);
}

void test_valid_a_record_response()
{
    uint8_t response[kCaptiveDnsPacketSize] = {};
    int response_len = build_captive_dns_response(kExampleQuery,
                                                  sizeof(kExampleQuery),
                                                  response,
                                                  sizeof(response));
    assert(response_len == static_cast<int>(sizeof(kExampleQuery) + sizeof(kExpectedAnswer)));
    assert(response[0] == 0x12 && response[1] == 0x34);
    assert(response[2] == 0x81 && response[3] == 0x80);
    assert(response[4] == 0x00 && response[5] == 0x01);
    assert(response[6] == 0x00 && response[7] == 0x01);
    assert(response[8] == 0x00 && response[9] == 0x00);
    assert(response[10] == 0x00 && response[11] == 0x00);
    assert(memcmp(response + 12, kExampleQuery + 12, sizeof(kExampleQuery) - 12) == 0);
    assert(memcmp(response + sizeof(kExampleQuery),
                  kExpectedAnswer,
                  sizeof(kExpectedAnswer)) == 0);
}

void test_in_place_a_record_response()
{
    uint8_t expected[kCaptiveDnsPacketSize] = {};
    int expected_len = build_captive_dns_response(kExampleQuery,
                                                  sizeof(kExampleQuery),
                                                  expected,
                                                  sizeof(expected));
    assert(expected_len > 0);

    uint8_t packet[kCaptiveDnsPacketSize] = {};
    memcpy(packet, kExampleQuery, sizeof(kExampleQuery));
    int packet_len = build_captive_dns_response(packet,
                                                sizeof(kExampleQuery),
                                                packet,
                                                sizeof(packet));
    assert(packet_len == expected_len);
    assert(memcmp(packet, expected, expected_len) == 0);
}
} // namespace

int main()
{
    test_invalid_arguments();
    test_invalid_queries();
    test_valid_a_record_response();
    test_in_place_a_record_response();
    return 0;
}
