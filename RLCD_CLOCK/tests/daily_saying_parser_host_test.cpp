// 验证每日文字 JSON/纯文本解析、字段优先级和 UTF-8 字符限制。
#include "ascii_text.h"
#include "daily_saying_parser.h"

#include <assert.h>
#include <string.h>
#include <string>

namespace {

void expect_text(const char *response, const char *expected)
{
    char out[128] = {};
    assert(daily_saying_parser::extract(response, out, sizeof(out)));
    assert(strcmp(out, expected) == 0);
}

std::string nested_data_json(int levels)
{
    std::string json = "\"深层文字\"";
    for (int i = 0; i < levels; ++i) {
        json = "{\"data\":" + json + "}";
    }
    return json;
}

} // namespace

int main()
{
    char trim_text[] = " \t  保留中间 空格 \r\n";
    trim_ascii_whitespace(trim_text);
    assert(strcmp(trim_text, "保留中间 空格") == 0);
    trim_ascii_whitespace(nullptr);

    expect_text("  今日宜从容  \n", "今日宜从容");
    expect_text("{\"content\":\"  山高水长  \"}", "山高水长");
    expect_text("{\"data\":{\"text\":\"一切顺利\"}}", "一切顺利");
    expect_text("{\"content\":\"优先\",\"text\":\"后选\"}", "优先");
    expect_text("{\"content\":\"   \",\"text\":\"空白后选\"}", "空白后选");
    expect_text("{\"data\":{\"content\":\"\\t\",\"text\":\"嵌套后选\"}}",
                "嵌套后选");
    expect_text("\"根字符串\"", "根字符串");
    const std::string deepest_allowed = nested_data_json(9);
    expect_text(deepest_allowed.c_str(), "深层文字");

    char out[128] = "old";
    assert(!daily_saying_parser::extract("{\"unknown\":\"value\"}", out, sizeof(out)));
    assert(out[0] == '\0');
    assert(!daily_saying_parser::extract("[\"array\"]", out, sizeof(out)));
    const std::string too_deep = nested_data_json(10);
    assert(!daily_saying_parser::extract(too_deep.c_str(), out, sizeof(out)));
    assert(out[0] == '\0');
    assert(!daily_saying_parser::extract("   \t\n", out, sizeof(out)));
    strcpy(out, "old");
    assert(!daily_saying_parser::extract(nullptr, out, sizeof(out)));
    assert(out[0] == '\0');
    assert(!daily_saying_parser::extract("text", nullptr, 0));

    const char *twenty_two = "一二三四五六七八九十甲乙丙丁戊己庚辛壬癸子丑";
    const char *twenty_three = "一二三四五六七八九十甲乙丙丁戊己庚辛壬癸子丑寅";
    int chars = -1;
    assert(daily_saying_parser::utf8_char_count(twenty_two) == 22);
    assert(daily_saying_parser::within_length(twenty_two, &chars));
    assert(chars == 22);
    assert(!daily_saying_parser::within_length(twenty_three, &chars));
    assert(chars == 23);
    assert(daily_saying_parser::utf8_char_count(nullptr) == 0);
    return 0;
}
