// 验证 HTTP 文本响应容量、Content-Length 和事件溢出的完整性规则。
#include "http_response_policy.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    constexpr size_t kBufferCapacity = 16;
    constexpr size_t kPayloadCapacity = kBufferCapacity - 1;

    assert(!http_response_is_truncated(false, -1, kBufferCapacity));
    assert(!http_response_is_truncated(false, 0, kBufferCapacity));
    assert(!http_response_is_truncated(false,
                                       static_cast<int64_t>(kPayloadCapacity),
                                       kBufferCapacity));
    assert(http_response_is_truncated(false,
                                      static_cast<int64_t>(kBufferCapacity),
                                      kBufferCapacity));
    assert(http_response_is_truncated(true, -1, kBufferCapacity));
    assert(http_response_is_truncated(true, 0, kBufferCapacity));
    assert(http_response_is_truncated(false, 0, 0));
    assert(http_response_is_truncated(false, 1, 1));
    assert(!http_response_is_truncated(false, 0, 1));
    return 0;
}
