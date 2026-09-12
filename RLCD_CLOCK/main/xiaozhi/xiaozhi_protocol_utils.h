// 声明小智协议使用的 UTF-8、WebSocket URL 和固定文本报文纯工具。
#pragma once

#include "xiaozhi_text_utils.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace xiaozhi_protocol {

[[maybe_unused]] static __attribute__((noinline)) size_t audio_frame_header_size(int version)
{
    if (version == 2) {
        return 16;
    }
    if (version == 3) {
        return 4;
    }
    return 0;
}

[[maybe_unused]] static __attribute__((noinline)) bool audio_frame_size(int version,
                                                       size_t payload_len,
                                                       size_t frame_capacity,
                                                       size_t *frame_len)
{
    if (!frame_len || payload_len == 0) {
        return false;
    }
    const size_t header_len = audio_frame_header_size(version);
    if ((version == 3 && payload_len > std::numeric_limits<uint16_t>::max()) ||
        (version == 2 && payload_len > std::numeric_limits<uint32_t>::max()) ||
        header_len > frame_capacity ||
        payload_len > frame_capacity - header_len) {
        return false;
    }
    *frame_len = header_len + payload_len;
    return true;
}

[[maybe_unused]] static __attribute__((noinline)) bool decoded_audio_size_valid(size_t decoded_size,
                                                               size_t buffer_capacity)
{
    return decoded_size > 0 &&
           decoded_size <= buffer_capacity &&
           decoded_size % sizeof(int16_t) == 0;
}

[[maybe_unused]] static __attribute__((noinline)) bool audio_sample_count_valid(
    size_t sample_count,
    size_t buffer_capacity)
{
    return sample_count > 0 && sample_count <= buffer_capacity;
}

[[maybe_unused]] static __attribute__((noinline)) bool audio_payload_range(int version,
                                                          const uint8_t *frame,
                                                          size_t frame_len,
                                                          size_t *payload_offset,
                                                          size_t *payload_len)
{
    if (!frame || frame_len == 0 || !payload_offset || !payload_len) {
        return false;
    }
    *payload_offset = 0;
    *payload_len = 0;
    const size_t header_len = audio_frame_header_size(version);
    if (header_len == 0) {
        *payload_len = frame_len;
        return true;
    }
    if (frame_len <= header_len) {
        return false;
    }
    size_t declared_len = 0;
    if (version == 2) {
        declared_len = (static_cast<size_t>(frame[12]) << 24) |
                       (static_cast<size_t>(frame[13]) << 16) |
                       (static_cast<size_t>(frame[14]) << 8) |
                       static_cast<size_t>(frame[15]);
    } else if (version == 3) {
        declared_len = (static_cast<size_t>(frame[2]) << 8) |
                       static_cast<size_t>(frame[3]);
    }
    if (declared_len == 0 || declared_len > frame_len - header_len) {
        return false;
    }
    *payload_offset = header_len;
    *payload_len = declared_len;
    return true;
}

[[maybe_unused]] static __attribute__((noinline)) bool parse_websocket_url(const char *url,
                                bool *secure,
                                char *host,
                                size_t host_len,
                                int *port,
                                char *path,
                                size_t path_len)
{
    if (!url || !secure || !output_buffer_available(host, host_len) || !port ||
        !output_buffer_available(path, path_len)) {
        return false;
    }
    const char *cursor = nullptr;
    if (strncmp(url, "wss://", 6) == 0) {
        *secure = true;
        cursor = url + 6;
        *port = 443;
    } else if (strncmp(url, "ws://", 5) == 0) {
        *secure = false;
        cursor = url + 5;
        *port = 80;
    } else {
        return false;
    }
    const char *path_start = strchr(cursor, '/');
    const char *host_end = path_start ? path_start : cursor + strlen(cursor);
    const char *colon = nullptr;
    for (const char *it = cursor; it < host_end; ++it) {
        if (*it == ':') {
            colon = it;
        }
    }
    size_t copied_host = static_cast<size_t>((colon ? colon : host_end) - cursor);
    if (copied_host == 0 || copied_host >= host_len) {
        return false;
    }
    memcpy(host, cursor, copied_host);
    host[copied_host] = '\0';
    if (colon) {
        char *end = nullptr;
        long parsed_port = strtol(colon + 1, &end, 10);
        if (end != host_end || parsed_port <= 0 || parsed_port > 65535) {
            return false;
        }
        *port = static_cast<int>(parsed_port);
    }
    strlcpy(path, path_start ? path_start : "/", path_len);
    return true;
}

[[maybe_unused]] static __attribute__((noinline)) void format_listen_start(char *out,
                                                          size_t out_len,
                                                          const char *session_id)
{
    if (!output_buffer_available(out, out_len) || !session_id) {
        return;
    }
    snprintf(out,
             out_len,
             "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"auto\"}",
             session_id);
}

[[maybe_unused]] static __attribute__((noinline)) void format_wake_abort(char *out,
                                                        size_t out_len,
                                                        const char *session_id)
{
    if (!output_buffer_available(out, out_len) || !session_id) {
        return;
    }
    snprintf(out,
             out_len,
             "{\"session_id\":\"%s\",\"type\":\"abort\",\"reason\":\"wake_word_detected\"}",
             session_id);
}

[[maybe_unused]] static __attribute__((noinline)) void format_websocket_headers(char *out,
                                                               size_t out_len,
                                                               int version,
                                                               const char *device_id,
                                                               const char *client_id)
{
    if (!output_buffer_available(out, out_len) || !device_id || !client_id) {
        return;
    }
    snprintf(out,
             out_len,
             "Protocol-Version: %d\r\nDevice-Id: %s\r\nClient-Id: %s\r\n",
             version,
             device_id,
             client_id);
}

[[maybe_unused]] static __attribute__((noinline)) void format_websocket_authorization(char *out,
                                                                     size_t out_len,
                                                                     const char *token)
{
    if (!output_buffer_available(out, out_len) || !token) {
        return;
    }
    snprintf(out, out_len, "%s%s", strchr(token, ' ') ? "" : "Bearer ", token);
}

[[maybe_unused]] static __attribute__((noinline)) void format_client_hello(char *out,
                                                          size_t out_len,
                                                          int version)
{
    if (!output_buffer_available(out, out_len)) {
        return;
    }
    snprintf(out,
             out_len,
             "{\"type\":\"hello\",\"version\":%d,\"features\":{\"mcp\":true},"
             "\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\","
             "\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}",
             version);
}

} // namespace xiaozhi_protocol
