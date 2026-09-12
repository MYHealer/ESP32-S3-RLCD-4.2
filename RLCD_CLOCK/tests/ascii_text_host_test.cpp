// 验证通用 ASCII 空白与单行配置空白裁剪边界。
#include "ascii_text.h"

#include <assert.h>
#include <string.h>

int main()
{
    trim_ascii_whitespace(nullptr);
    trim_ascii_line_whitespace(nullptr);

    char empty[] = "";
    trim_ascii_whitespace(empty);
    assert(strcmp(empty, "") == 0);

    char ordinary[] = " \t\r\n杭州 \t\r\n";
    trim_ascii_line_whitespace(ordinary);
    assert(strcmp(ordinary, "杭州") == 0);

    char c_space[] = "\v\fvalue\f\v";
    trim_ascii_whitespace(c_space);
    assert(strcmp(c_space, "value") == 0);

    char line_space[] = "\v\fvalue\f\v";
    trim_ascii_line_whitespace(line_space);
    assert(strcmp(line_space, "\v\fvalue\f\v") == 0);

    char only_line_space[] = " \t\r\n";
    trim_ascii_line_whitespace(only_line_space);
    assert(strcmp(only_line_space, "") == 0);

    char unchanged[] = "Shanghai";
    trim_ascii_whitespace(unchanged);
    assert(strcmp(unchanged, "Shanghai") == 0);
    return 0;
}
