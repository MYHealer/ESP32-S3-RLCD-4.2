// 定义 QWeather 每日预报从 7 日接口降级到 3 日接口的纯策略。
#pragma once

enum class QweatherDailyAttemptStatus {
    kSuccess = 0,
    kShorterEndpointAllowed = 1,
    kFailed = 2,
};

QweatherDailyAttemptStatus qweather_daily_attempt_status(bool parsed_success,
                                                         const char *response_code);

constexpr bool qweather_daily_should_try_shorter(
    QweatherDailyAttemptStatus status)
{
    return status == QweatherDailyAttemptStatus::kShorterEndpointAllowed;
}
