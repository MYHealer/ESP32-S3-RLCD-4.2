// 验证 QWeather 图标范围、精确代码和 UTF-8 文本映射边界。
#include "qweather_icons.h"

#include <assert.h>
#include <stdint.h>

namespace {

uint32_t decode_three_byte_utf8(const char *text)
{
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(text);
    assert(bytes != nullptr);
    assert((bytes[0] & 0xF0U) == 0xE0U);
    assert((bytes[1] & 0xC0U) == 0x80U);
    assert((bytes[2] & 0xC0U) == 0x80U);
    assert(bytes[3] == '\0');
    return static_cast<uint32_t>(bytes[0] & 0x0FU) << 12U |
           static_cast<uint32_t>(bytes[1] & 0x3FU) << 6U |
           static_cast<uint32_t>(bytes[2] & 0x3FU);
}

} // namespace

int main()
{
    assert(weather_icon_codepoint(nullptr) == 0xF146U);
    assert(weather_icon_codepoint("") == 0xF146U);
    assert(weather_icon_codepoint("invalid") == 0xF146U);

    assert(weather_icon_codepoint("100") == 0xF101U);
    assert(weather_icon_codepoint("104") == 0xF105U);
    assert(weather_icon_codepoint("150") == 0xF106U);
    assert(weather_icon_codepoint("318") == 0xF11CU);
    assert(weather_icon_codepoint("399") == 0xF11FU);
    assert(weather_icon_codepoint("499") == 0xF12DU);
    assert(weather_icon_codepoint("900") == 0xF144U);
    assert(weather_icon_codepoint("901") == 0xF145U);
    assert(weather_icon_codepoint("9999") == 0xF1CBU);
    assert(weather_icon_codepoint("799") == 0xF146U);

    WeatherIconText sunny = weather_icon_text("100");
    WeatherIconText unknown = weather_icon_text("9999");
    assert(decode_three_byte_utf8(sunny.c_str()) == 0xF101U);
    assert(decode_three_byte_utf8(unknown.c_str()) == 0xF1CBU);
    assert(decode_three_byte_utf8(sunny.c_str()) == 0xF101U);
    return 0;
}
