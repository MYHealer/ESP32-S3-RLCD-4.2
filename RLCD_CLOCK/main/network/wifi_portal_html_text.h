// 声明配网页 HTML 正文和属性值共用的文本转义接口。
#pragma once

#include <stddef.h>

namespace wifi_portal_html {

void escape_text(const char *src, char *dst, size_t dst_len);

} // namespace wifi_portal_html
