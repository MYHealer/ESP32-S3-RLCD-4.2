// 声明小智协议和页面快照共用的有界文本工具。
#pragma once

#include <stddef.h>

namespace xiaozhi_protocol {

bool output_buffer_available(char *out, size_t out_len);
void utf8_safe_copy(char *out, size_t out_len, const char *text);

} // namespace xiaozhi_protocol
