// 验证公共文本输出缓冲区与格式化结果的边界判断。
#include "app_text_format.h"

#include <assert.h>

int main()
{
    char output[8] = {};

    static_assert(app_text::output_buffer_available(output, sizeof(output)));
    static_assert(!app_text::output_buffer_available(nullptr, sizeof(output)));
    static_assert(!app_text::output_buffer_available(output, 0));

    static_assert(app_text::format_failed(-1, sizeof(output)));
    static_assert(!app_text::format_failed(0, sizeof(output)));
    static_assert(!app_text::format_failed(7, sizeof(output)));
    static_assert(app_text::format_failed(8, sizeof(output)));
    static_assert(app_text::format_failed(9, sizeof(output)));
    static_assert(app_text::format_failed(0, 0));

    assert(app_text::output_buffer_available(output, sizeof(output)));
    return 0;
}
