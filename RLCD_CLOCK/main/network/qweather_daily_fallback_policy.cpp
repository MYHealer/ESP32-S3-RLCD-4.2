// 将 QWeather 业务码归类为成功、可降级或不可立即重试。
#include "qweather_daily_fallback_policy.h"

#include <string.h>

namespace {
constexpr const char *kQweatherLegacyNoAccessCode = "403";
}

QweatherDailyAttemptStatus qweather_daily_attempt_status(
    bool parsed_success,
    const char *response_code)
{
    if (parsed_success) {
        return QweatherDailyAttemptStatus::kSuccess;
    }
    if (response_code &&
        strcmp(response_code, kQweatherLegacyNoAccessCode) == 0) {
        return QweatherDailyAttemptStatus::kShorterEndpointAllowed;
    }
    return QweatherDailyAttemptStatus::kFailed;
}
