// 声明 QWeather 图标代码到字体码点和独立 UTF-8 文本值的映射接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t kWeatherIconUtf8TextSize = 5;

struct WeatherIconText {
    char value[kWeatherIconUtf8TextSize] = {};

    const char *c_str() const
    {
        return value;
    }
};

uint32_t weather_icon_codepoint(const char *code);
WeatherIconText weather_icon_text(const char *code);
