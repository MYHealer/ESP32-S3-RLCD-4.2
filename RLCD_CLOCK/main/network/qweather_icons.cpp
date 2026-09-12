// 将 QWeather 图标代码映射为项目天气字体使用的私有区码点。
#include "qweather_icons.h"

#include "app_constexpr.h"
#include "app_text_format.h"

#include <stddef.h>
#include <stdlib.h>

namespace {
constexpr uint32_t kWeatherIconDefaultCodepoint = 0xF146;
constexpr int kWeatherIconSunnyStart = 100;
constexpr int kWeatherIconSunnyEnd = 104;
constexpr uint32_t kWeatherIconSunnyBaseCodepoint = 0xF101;
constexpr int kWeatherIconNightSunnyStart = 150;
constexpr int kWeatherIconNightSunnyEnd = 153;
constexpr uint32_t kWeatherIconNightSunnyBaseCodepoint = 0xF106;
constexpr int kWeatherIconRainStart = 300;
constexpr int kWeatherIconRainEnd = 318;
constexpr uint32_t kWeatherIconRainBaseCodepoint = 0xF10A;
constexpr int kWeatherIconNightRainStart = 350;
constexpr int kWeatherIconNightRainEnd = 351;
constexpr uint32_t kWeatherIconNightRainBaseCodepoint = 0xF11D;
constexpr int kWeatherIconRainUnknownCode = 399;
constexpr uint32_t kWeatherIconRainUnknownCodepoint = 0xF11F;
constexpr int kWeatherIconSnowStart = 400;
constexpr int kWeatherIconSnowEnd = 410;
constexpr uint32_t kWeatherIconSnowBaseCodepoint = 0xF120;
constexpr int kWeatherIconNightSnowStart = 456;
constexpr int kWeatherIconNightSnowEnd = 457;
constexpr uint32_t kWeatherIconNightSnowBaseCodepoint = 0xF12B;
constexpr int kWeatherIconSnowUnknownCode = 499;
constexpr uint32_t kWeatherIconSnowUnknownCodepoint = 0xF12D;
constexpr int kWeatherIconFogStart = 500;
constexpr int kWeatherIconFogEnd = 504;
constexpr uint32_t kWeatherIconFogBaseCodepoint = 0xF12E;
constexpr int kWeatherIconDustStart = 507;
constexpr int kWeatherIconDustEnd = 515;
constexpr uint32_t kWeatherIconDustBaseCodepoint = 0xF133;
constexpr int kWeatherIconCloudStart = 800;
constexpr int kWeatherIconCloudEnd = 807;
constexpr uint32_t kWeatherIconCloudBaseCodepoint = 0xF13C;
constexpr int kWeatherIconHotCode = 900;
constexpr uint32_t kWeatherIconHotCodepoint = 0xF144;
constexpr int kWeatherIconColdCode = 901;
constexpr uint32_t kWeatherIconColdCodepoint = 0xF145;
constexpr int kWeatherIconUnknownCode = 9999;
constexpr uint32_t kWeatherIconUnknownCodepoint = 0xF1CB;

constexpr uint32_t kUtf8OneByteMaxCodepoint = 0x7F;
constexpr uint32_t kUtf8TwoByteMaxCodepoint = 0x7FF;
constexpr uint32_t kUtf8ThreeByteMaxCodepoint = 0xFFFF;
constexpr unsigned char kUtf8TwoBytePrefix = 0xC0;
constexpr unsigned char kUtf8ThreeBytePrefix = 0xE0;
constexpr unsigned char kUtf8FourBytePrefix = 0xF0;
constexpr unsigned char kUtf8ContinuationPrefix = 0x80;
constexpr uint32_t kUtf8ContinuationPayloadMask = 0x3F;
constexpr int kUtf8Shift6 = 6;
constexpr int kUtf8Shift12 = 12;
constexpr int kUtf8Shift18 = 18;
constexpr size_t kUtf8OneByteLen = 1;
constexpr size_t kUtf8TwoByteLen = 2;
constexpr size_t kUtf8ThreeByteLen = 3;
constexpr size_t kUtf8FourByteLen = 4;

struct WeatherIconRange {
    int first;
    int last;
    uint32_t base_codepoint;
};

struct WeatherIconExact {
    int code;
    uint32_t codepoint;
};

constexpr WeatherIconRange kWeatherIconRanges[] = {
    {kWeatherIconSunnyStart, kWeatherIconSunnyEnd, kWeatherIconSunnyBaseCodepoint},
    {kWeatherIconNightSunnyStart, kWeatherIconNightSunnyEnd, kWeatherIconNightSunnyBaseCodepoint},
    {kWeatherIconRainStart, kWeatherIconRainEnd, kWeatherIconRainBaseCodepoint},
    {kWeatherIconNightRainStart, kWeatherIconNightRainEnd, kWeatherIconNightRainBaseCodepoint},
    {kWeatherIconSnowStart, kWeatherIconSnowEnd, kWeatherIconSnowBaseCodepoint},
    {kWeatherIconNightSnowStart, kWeatherIconNightSnowEnd, kWeatherIconNightSnowBaseCodepoint},
    {kWeatherIconFogStart, kWeatherIconFogEnd, kWeatherIconFogBaseCodepoint},
    {kWeatherIconDustStart, kWeatherIconDustEnd, kWeatherIconDustBaseCodepoint},
    {kWeatherIconCloudStart, kWeatherIconCloudEnd, kWeatherIconCloudBaseCodepoint},
};

constexpr WeatherIconExact kWeatherIconExactCodes[] = {
    {kWeatherIconRainUnknownCode, kWeatherIconRainUnknownCodepoint},
    {kWeatherIconSnowUnknownCode, kWeatherIconSnowUnknownCodepoint},
    {kWeatherIconHotCode, kWeatherIconHotCodepoint},
    {kWeatherIconColdCode, kWeatherIconColdCodepoint},
    {kWeatherIconUnknownCode, kWeatherIconUnknownCodepoint},
};

constexpr bool weather_icon_range_table_valid()
{
    for (const WeatherIconRange &range : kWeatherIconRanges) {
        if (range.first > range.last || range.base_codepoint == 0) {
            return false;
        }
    }
    return true;
}

constexpr bool weather_icon_exact_table_valid()
{
    for (const WeatherIconExact &exact : kWeatherIconExactCodes) {
        if (exact.code < 0 || exact.codepoint == 0) {
            return false;
        }
    }
    return true;
}

uint32_t weather_icon_range_codepoint(int icon, const WeatherIconRange &range)
{
    if (icon < range.first || icon > range.last) {
        return 0;
    }
    return range.base_codepoint + static_cast<uint32_t>(icon - range.first);
}

void write_weather_icon_utf8(char *out, size_t out_len, uint32_t cp)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    out[0] = '\0';
    if (cp <= kUtf8OneByteMaxCodepoint) {
        if (out_len <= kUtf8OneByteLen) {
            return;
        }
        out[0] = static_cast<char>(cp);
        out[1] = '\0';
        return;
    }
    if (cp <= kUtf8TwoByteMaxCodepoint) {
        if (out_len <= kUtf8TwoByteLen) {
            return;
        }
        out[0] = static_cast<char>(kUtf8TwoBytePrefix | (cp >> kUtf8Shift6));
        out[1] = static_cast<char>(kUtf8ContinuationPrefix | (cp & kUtf8ContinuationPayloadMask));
        out[2] = '\0';
        return;
    }
    if (cp <= kUtf8ThreeByteMaxCodepoint) {
        if (out_len <= kUtf8ThreeByteLen) {
            return;
        }
        out[0] = static_cast<char>(kUtf8ThreeBytePrefix | (cp >> kUtf8Shift12));
        out[1] = static_cast<char>(kUtf8ContinuationPrefix |
                                   ((cp >> kUtf8Shift6) & kUtf8ContinuationPayloadMask));
        out[2] = static_cast<char>(kUtf8ContinuationPrefix | (cp & kUtf8ContinuationPayloadMask));
        out[3] = '\0';
        return;
    }
    if (out_len <= kUtf8FourByteLen) {
        return;
    }
    out[0] = static_cast<char>(kUtf8FourBytePrefix | (cp >> kUtf8Shift18));
    out[1] = static_cast<char>(kUtf8ContinuationPrefix |
                               ((cp >> kUtf8Shift12) & kUtf8ContinuationPayloadMask));
    out[2] = static_cast<char>(kUtf8ContinuationPrefix |
                               ((cp >> kUtf8Shift6) & kUtf8ContinuationPayloadMask));
    out[3] = static_cast<char>(kUtf8ContinuationPrefix | (cp & kUtf8ContinuationPayloadMask));
    out[4] = '\0';
}

static_assert(kWeatherIconUtf8TextSize > kUtf8FourByteLen,
              "weather icon UTF-8 text buffer must fit a four-byte codepoint and NUL");
static_assert(sizeof(WeatherIconText) == kWeatherIconUtf8TextSize,
              "weather icon text value must contain only its UTF-8 bytes");
static_assert(kWeatherIconSunnyStart <= kWeatherIconSunnyEnd,
              "sunny weather icon range must be ordered");
static_assert(kWeatherIconNightSunnyStart <= kWeatherIconNightSunnyEnd,
              "night sunny weather icon range must be ordered");
static_assert(kWeatherIconRainStart <= kWeatherIconRainEnd,
              "rain weather icon range must be ordered");
static_assert(kWeatherIconNightRainStart <= kWeatherIconNightRainEnd,
              "night rain weather icon range must be ordered");
static_assert(kWeatherIconSnowStart <= kWeatherIconSnowEnd,
              "snow weather icon range must be ordered");
static_assert(kWeatherIconNightSnowStart <= kWeatherIconNightSnowEnd,
              "night snow weather icon range must be ordered");
static_assert(kWeatherIconFogStart <= kWeatherIconFogEnd,
              "fog weather icon range must be ordered");
static_assert(kWeatherIconDustStart <= kWeatherIconDustEnd,
              "dust weather icon range must be ordered");
static_assert(kWeatherIconCloudStart <= kWeatherIconCloudEnd,
              "cloud weather icon range must be ordered");
static_assert(kUtf8OneByteLen < kUtf8TwoByteLen &&
                  kUtf8TwoByteLen < kUtf8ThreeByteLen &&
                  kUtf8ThreeByteLen < kUtf8FourByteLen,
              "UTF-8 byte length constants must stay ordered");
static_assert(kUtf8OneByteMaxCodepoint < kUtf8TwoByteMaxCodepoint &&
                  kUtf8TwoByteMaxCodepoint < kUtf8ThreeByteMaxCodepoint,
              "UTF-8 codepoint limits must stay ordered");
static_assert(array_count(kWeatherIconRanges) > 0, "weather icon range table must not be empty");
static_assert(array_count(kWeatherIconExactCodes) > 0, "weather icon exact code table must not be empty");
static_assert(weather_icon_range_table_valid(), "weather icon range entries must be ordered and complete");
static_assert(weather_icon_exact_table_valid(), "weather icon exact entries must be complete");
} // namespace

uint32_t weather_icon_codepoint(const char *code)
{
    if (!code || code[0] == '\0') {
        return kWeatherIconDefaultCodepoint;
    }
    int icon = atoi(code);
    for (const WeatherIconRange &range : kWeatherIconRanges) {
        uint32_t codepoint = weather_icon_range_codepoint(icon, range);
        if (codepoint != 0) {
            return codepoint;
        }
    }
    for (const WeatherIconExact &exact : kWeatherIconExactCodes) {
        if (icon == exact.code) {
            return exact.codepoint;
        }
    }
    return kWeatherIconDefaultCodepoint;
}

WeatherIconText weather_icon_text(const char *code)
{
    WeatherIconText text = {};
    write_weather_icon_utf8(text.value,
                            sizeof(text.value),
                            weather_icon_codepoint(code));
    return text;
}
