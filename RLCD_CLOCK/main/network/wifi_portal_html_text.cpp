// 转义配网页正文和属性值中的 HTML 特殊字符。
#include "wifi_portal_html_text.h"

#include "app_text_format.h"

#include <string.h>

namespace wifi_portal_html {

void escape_text(const char *src, char *dst, size_t dst_len)
{
    if (!app_text::output_buffer_available(dst, dst_len)) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t dst_index = 0;
    for (size_t src_index = 0;
         src[src_index] != '\0' && dst_index + 1 < dst_len;
         ++src_index) {
        const char *replacement = nullptr;
        if (src[src_index] == '&') {
            replacement = "&amp;";
        } else if (src[src_index] == '<') {
            replacement = "&lt;";
        } else if (src[src_index] == '>') {
            replacement = "&gt;";
        } else if (src[src_index] == '"') {
            replacement = "&quot;";
        } else if (src[src_index] == '\'') {
            replacement = "&#39;";
        }
        if (replacement) {
            const size_t replacement_len = strlen(replacement);
            if (dst_index + replacement_len >= dst_len) {
                break;
            }
            memcpy(dst + dst_index, replacement, replacement_len);
            dst_index += replacement_len;
        } else {
            dst[dst_index++] = src[src_index];
        }
    }
    dst[dst_index] = '\0';
}

} // namespace wifi_portal_html
