// 在主机侧验证天气城市中文后缀、空白、非法字符和长度边界。
#include "weather_city_text.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
[[noreturn]] void fail(const char *message)
{
    std::fprintf(stderr, "Weather city text test failed: %s\n", message);
    std::exit(1);
}

void expect_normalized(const char *input, const char *expected)
{
    char out[64] = {};
    if (!weather_city_text::normalize(input, out, sizeof(out)) ||
        std::strcmp(out, expected) != 0) {
        fail("normalization mismatch");
    }
}
} // namespace

int main()
{
    expect_normalized("杭州", "杭州");
    expect_normalized("杭州市", "杭州");
    expect_normalized(" 上海市 ", "上海");
    expect_normalized("临平区", "临平区");

    char out[64] = {};
    if (weather_city_text::normalize("杭州/上海", out, sizeof(out))) {
        fail("invalid separator was accepted");
    }
    char too_long[80] = {};
    std::memset(too_long, 'a', sizeof(too_long) - 1);
    if (weather_city_text::normalize(too_long, out, sizeof(out))) {
        fail("oversized city was accepted");
    }
    std::puts("Weather city text host tests passed");
    return 0;
}
