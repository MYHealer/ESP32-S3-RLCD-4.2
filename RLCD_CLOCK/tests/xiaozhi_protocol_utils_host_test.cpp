// 验证小智协议 UTF-8、URL 和固定 WebSocket 文本工具的既有语义。
#include "xiaozhi_protocol_utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
void expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void test_utf8_safe_copy()
{
    char out[16] = {};
    xiaozhi_protocol::utf8_safe_copy(out, sizeof(out), "杭州");
    expect(std::strcmp(out, "杭州") == 0, "valid UTF-8 copy changed");

    char short_out[5] = {};
    xiaozhi_protocol::utf8_safe_copy(short_out, sizeof(short_out), "杭州");
    expect(std::strcmp(short_out, "杭") == 0, "UTF-8 character was split");

    const char invalid[] = {'A', static_cast<char>(0xff), 'B', '\0'};
    xiaozhi_protocol::utf8_safe_copy(out, sizeof(out), invalid);
    expect(std::strcmp(out, "A?B") == 0, "invalid UTF-8 replacement changed");

    std::strcpy(out, "unchanged");
    xiaozhi_protocol::utf8_safe_copy(nullptr, 0, "ignored");
    xiaozhi_protocol::utf8_safe_copy(out, sizeof(out), nullptr);
    expect(out[0] == '\0', "null UTF-8 source did not clear output");
}

void test_websocket_url()
{
    bool secure = false;
    char host[64] = {};
    char path[64] = {};
    int port = 0;
    expect(xiaozhi_protocol::parse_websocket_url(
               "wss://example.com/xiaozhi", &secure, host, sizeof(host), &port, path, sizeof(path)),
           "secure WebSocket URL rejected");
    expect(secure && port == 443 && std::strcmp(host, "example.com") == 0 &&
               std::strcmp(path, "/xiaozhi") == 0,
           "secure WebSocket URL fields changed");

    expect(xiaozhi_protocol::parse_websocket_url(
               "ws://localhost:8080", &secure, host, sizeof(host), &port, path, sizeof(path)),
           "custom WebSocket port rejected");
    expect(!secure && port == 8080 && std::strcmp(host, "localhost") == 0 &&
               std::strcmp(path, "/") == 0,
           "custom WebSocket URL fields changed");

    expect(!xiaozhi_protocol::parse_websocket_url(
               "https://example.com", &secure, host, sizeof(host), &port, path, sizeof(path)),
           "non-WebSocket scheme accepted");
    expect(!xiaozhi_protocol::parse_websocket_url(
               "ws://example.com:0", &secure, host, sizeof(host), &port, path, sizeof(path)),
           "zero WebSocket port accepted");
    expect(!xiaozhi_protocol::parse_websocket_url(
               "ws:///path", &secure, host, sizeof(host), &port, path, sizeof(path)),
           "empty WebSocket host accepted");
}

void test_websocket_messages()
{
    char out[256] = {};
    xiaozhi_protocol::format_listen_start(out, sizeof(out), "session-1");
    expect(std::strcmp(out,
                       "{\"session_id\":\"session-1\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"auto\"}") == 0,
           "listen-start payload changed");

    xiaozhi_protocol::format_wake_abort(out, sizeof(out), "session-1");
    expect(std::strcmp(out,
                       "{\"session_id\":\"session-1\",\"type\":\"abort\",\"reason\":\"wake_word_detected\"}") == 0,
           "wake-abort payload changed");

    xiaozhi_protocol::format_websocket_headers(
        out, sizeof(out), 3, "device-id", "client-id");
    expect(std::strcmp(out,
                       "Protocol-Version: 3\r\nDevice-Id: device-id\r\nClient-Id: client-id\r\n") == 0,
           "WebSocket headers changed");

    xiaozhi_protocol::format_websocket_authorization(out, sizeof(out), "token-value");
    expect(std::strcmp(out, "Bearer token-value") == 0,
           "Bearer authorization prefix changed");
    xiaozhi_protocol::format_websocket_authorization(out, sizeof(out), "Bearer token-value");
    expect(std::strcmp(out, "Bearer token-value") == 0,
           "existing authorization scheme changed");

    xiaozhi_protocol::format_client_hello(out, sizeof(out), 2);
    expect(std::strcmp(out,
                       "{\"type\":\"hello\",\"version\":2,\"features\":{\"mcp\":true},"
                       "\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\","
                       "\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}") == 0,
           "client hello payload changed");
}

void test_audio_frame_bounds()
{
    size_t frame_len = 0;
    expect(xiaozhi_protocol::audio_frame_header_size(1) == 0,
           "legacy audio header size changed");
    expect(xiaozhi_protocol::audio_frame_header_size(2) == 16,
           "protocol v2 audio header size changed");
    expect(xiaozhi_protocol::audio_frame_header_size(3) == 4,
           "protocol v3 audio header size changed");

    expect(xiaozhi_protocol::audio_frame_size(2, 1280, 1296, &frame_len) &&
               frame_len == 1296,
           "protocol v2 frame capacity rejected");
    expect(xiaozhi_protocol::audio_frame_size(3, 1280, 1296, &frame_len) &&
               frame_len == 1284,
           "protocol v3 frame capacity rejected");
    expect(xiaozhi_protocol::audio_frame_size(1, 1280, 1280, &frame_len) &&
               frame_len == 1280,
           "legacy frame capacity rejected");
    expect(!xiaozhi_protocol::audio_frame_size(2, 1281, 1296, &frame_len),
           "oversized v2 frame accepted");
    expect(!xiaozhi_protocol::audio_frame_size(3,
                                               static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1,
                                               70000,
                                               &frame_len),
           "v3 payload exceeding wire length accepted");
    expect(!xiaozhi_protocol::audio_frame_size(3, 0, 1296, &frame_len),
           "empty audio payload accepted");
    expect(!xiaozhi_protocol::audio_frame_size(3, 1, 1296, nullptr),
           "null frame length accepted");

    expect(xiaozhi_protocol::decoded_audio_size_valid(2, 2),
           "single decoded sample rejected");
    expect(xiaozhi_protocol::decoded_audio_size_valid(5760, 5760),
           "full decoded PCM buffer rejected");
    expect(!xiaozhi_protocol::decoded_audio_size_valid(0, 5760),
           "empty decoded PCM accepted");
    expect(!xiaozhi_protocol::decoded_audio_size_valid(3, 5760),
           "unaligned decoded PCM accepted");
    expect(!xiaozhi_protocol::decoded_audio_size_valid(5762, 5760),
           "oversized decoded PCM accepted");
    expect(xiaozhi_protocol::audio_sample_count_valid(1, 1600),
           "single converted PCM sample rejected");
    expect(xiaozhi_protocol::audio_sample_count_valid(1600, 1600),
           "full converted PCM buffer rejected");
    expect(!xiaozhi_protocol::audio_sample_count_valid(0, 1600),
           "empty converted PCM accepted");
    expect(!xiaozhi_protocol::audio_sample_count_valid(1601, 1600),
           "oversized converted PCM accepted");
}

void test_audio_payload_range()
{
    size_t offset = 99;
    size_t payload_len = 99;
    const uint8_t legacy[] = {1, 2, 3};
    expect(xiaozhi_protocol::audio_payload_range(
               1, legacy, sizeof(legacy), &offset, &payload_len) &&
               offset == 0 && payload_len == sizeof(legacy),
           "legacy audio payload range changed");

    uint8_t v2[20] = {};
    v2[15] = 4;
    expect(xiaozhi_protocol::audio_payload_range(
               2, v2, sizeof(v2), &offset, &payload_len) &&
               offset == 16 && payload_len == 4,
           "protocol v2 payload range rejected");
    v2[15] = 5;
    expect(!xiaozhi_protocol::audio_payload_range(
               2, v2, sizeof(v2), &offset, &payload_len),
           "protocol v2 oversized payload accepted");
    v2[15] = 0;
    expect(!xiaozhi_protocol::audio_payload_range(
               2, v2, sizeof(v2), &offset, &payload_len),
           "protocol v2 empty payload accepted");
    expect(!xiaozhi_protocol::audio_payload_range(2, v2, 16, &offset, &payload_len),
           "protocol v2 header-only frame accepted");

    uint8_t v3[9] = {};
    v3[3] = 3;
    expect(xiaozhi_protocol::audio_payload_range(
               3, v3, sizeof(v3), &offset, &payload_len) &&
               offset == 4 && payload_len == 3,
           "protocol v3 payload range rejected");
    v3[2] = 1;
    v3[3] = 0;
    expect(!xiaozhi_protocol::audio_payload_range(
               3, v3, sizeof(v3), &offset, &payload_len),
           "protocol v3 oversized payload accepted");

    expect(!xiaozhi_protocol::audio_payload_range(
               1, nullptr, 1, &offset, &payload_len),
           "null audio frame accepted");
    expect(!xiaozhi_protocol::audio_payload_range(
               1, legacy, sizeof(legacy), nullptr, &payload_len),
           "null payload offset accepted");
    expect(!xiaozhi_protocol::audio_payload_range(
               1, legacy, sizeof(legacy), &offset, nullptr),
           "null payload length accepted");
}
} // namespace

int main()
{
    test_utf8_safe_copy();
    test_websocket_url();
    test_websocket_messages();
    test_audio_frame_bounds();
    test_audio_payload_range();
    std::puts("Xiaozhi protocol utility host tests passed");
    return 0;
}
