// 将 QWeather 城市响应归类为成功、未找到或不可立即重试。
#include "qweather_city_lookup_retry_policy.h"

#include <string.h>

namespace {
constexpr const char *kQweatherSuccessCode = "200";
constexpr const char *kQweatherLocationNotFoundCode = "404";

bool response_code_equals(const char *actual, const char *expected)
{
    return actual && expected && strcmp(actual, expected) == 0;
}
} // namespace

QweatherCityLookupStatus qweather_city_lookup_response_status(
    bool parsed_success,
    bool location_array_available,
    bool location_candidate_available,
    const char *response_code)
{
    if (parsed_success) {
        return kQweatherCityLookupOk;
    }
    if (location_candidate_available) {
        return kQweatherCityLookupError;
    }
    if (response_code_equals(response_code, kQweatherLocationNotFoundCode) ||
        (location_array_available &&
         response_code_equals(response_code, kQweatherSuccessCode))) {
        return kQweatherCityLookupNotFound;
    }
    return kQweatherCityLookupError;
}
