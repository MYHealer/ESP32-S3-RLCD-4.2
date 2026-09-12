// 验证离线手动时间的格式、日期归一化和输入边界。
#include "manual_time_parser.h"

#include <assert.h>

namespace {

void expect_time(const char *text,
                 int year,
                 int month,
                 int day,
                 int hour,
                 int minute,
                 int second)
{
    struct tm parsed = {};
    assert(parse_manual_datetime_text(text, &parsed));
    assert(parsed.tm_year + kManualTimeTmYearOffset == year);
    assert(parsed.tm_mon + kManualTimeTmMonthOffset == month);
    assert(parsed.tm_mday == day);
    assert(parsed.tm_hour == hour);
    assert(parsed.tm_min == minute);
    assert(parsed.tm_sec == second);
}

} // namespace

int main()
{
    expect_time("2026-07-13T12:34:56", 2026, 7, 13, 12, 34, 56);
    expect_time("2026-07-13 12:34:56", 2026, 7, 13, 12, 34, 56);
    expect_time("2026-07-13 12:34", 2026, 7, 13, 12, 34, 0);
    expect_time("2024-02-29 00:00", 2024, 2, 29, 0, 0, 0);

    // 保留既有 sscanf 兼容性：已解析五个必需字段后允许尾随文本。
    expect_time("2026-07-13T12:34extra", 2026, 7, 13, 12, 34, 0);

    struct tm parsed = {};
    assert(!parse_manual_datetime_text(nullptr, &parsed));
    assert(!parse_manual_datetime_text("", &parsed));
    assert(!parse_manual_datetime_text("2026-07-13 12:34", nullptr));
    assert(!parse_manual_datetime_text("2023-12-31 23:59", &parsed));
    assert(!parse_manual_datetime_text("2036-01-01 00:00", &parsed));
    // 保留现有 mktime 规范化语义：无效月内日期会滚入下一月。
    expect_time("2026-02-29 12:00", 2026, 3, 1, 12, 0, 0);
    assert(!parse_manual_datetime_text("2026-13-01 12:00", &parsed));
    assert(!parse_manual_datetime_text("2026-07-13 24:00", &parsed));
    assert(!parse_manual_datetime_text("2026-07-13 12:60", &parsed));
    assert(!parse_manual_datetime_text("not-a-time", &parsed));
    return 0;
}
