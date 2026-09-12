// 声明每日文字响应解析和 UTF-8 字符数限制的纯逻辑接口。
#pragma once

#include <stddef.h>

namespace daily_saying_parser {

inline constexpr int kMaxChars = 22;

bool extract(const char *response, char *out, size_t out_len);
int utf8_char_count(const char *text);
bool within_length(const char *text, int *chars_out = nullptr);

} // namespace daily_saying_parser
