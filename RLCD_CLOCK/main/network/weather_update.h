// 声明天气同步结果、请求作用域和高层更新入口。
#pragma once

enum class WeatherUpdateResult {
    kSuccess,
    kFailed,
    kResourceDeferred,
};

enum class WeatherUpdateScope {
    kCurrentAndAlerts,
    kFull,
};

WeatherUpdateResult perform_weather_update(WeatherUpdateScope scope);
