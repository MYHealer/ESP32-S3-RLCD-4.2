// 提供 UI 文本复制和格式化失败回退的共享轻量工具。
#pragma once

#include "app_text_format.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace ui_text {

inline bool output_buffer_available(char *out, size_t out_len)
{
    return app_text::output_buffer_available(out, out_len);
}

inline bool format_failed(int written, size_t out_len)
{
    return app_text::format_failed(written, out_len);
}

inline void copy(char *out, size_t out_len, const char *text)
{
    if (!output_buffer_available(out, out_len)) {
        return;
    }
    strlcpy(out, text ? text : "", out_len);
}

template <typename... Args>
void format_or_fallback(char *out, size_t out_len, const char *fallback, const char *format, Args... args)
{
    if (!output_buffer_available(out, out_len) || !format) {
        if (!format) {
            copy(out, out_len, fallback);
        }
        return;
    }
    int written = snprintf(out, out_len, format, args...);
    if (format_failed(written, out_len)) {
        copy(out, out_len, fallback);
    }
}

} // namespace ui_text
