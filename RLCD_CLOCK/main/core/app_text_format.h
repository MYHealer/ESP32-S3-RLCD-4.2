// 提供跨业务模块复用的输出缓冲区与格式化结果边界判断。
#pragma once

#include <stddef.h>

namespace app_text {

constexpr bool output_buffer_available(const char *out, size_t out_len)
{
    return out && out_len > 0;
}

constexpr bool format_failed(int written, size_t out_len)
{
    return written < 0 || static_cast<size_t>(written) >= out_len;
}

} // namespace app_text
