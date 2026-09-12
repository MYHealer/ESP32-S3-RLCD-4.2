// 定义 HTTP 文本响应是否完整落入固定缓冲区的纯判定规则。
#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr bool http_response_is_truncated(bool event_overflow,
                                          int64_t content_length,
                                          size_t buffer_capacity)
{
    if (event_overflow || buffer_capacity == 0) {
        return true;
    }
    if (content_length < 0) {
        return false;
    }
    const size_t payload_capacity = buffer_capacity - 1;
    return static_cast<uint64_t>(content_length) >
           static_cast<uint64_t>(payload_capacity);
}

static_assert(!http_response_is_truncated(false, 15, 16),
              "a response that leaves room for NUL must fit");
static_assert(http_response_is_truncated(false, 16, 16),
              "a response without NUL capacity must be rejected");
static_assert(http_response_is_truncated(true, -1, 16),
              "event overflow must reject unknown-length responses");
