// 定义 QWeather 城市查询结果及备用查询词重试的纯策略。
#pragma once

enum QweatherCityLookupStatus {
    kQweatherCityLookupOk = 0,
    kQweatherCityLookupNotFound = 1,
    kQweatherCityLookupError = 2,
};

QweatherCityLookupStatus qweather_city_lookup_response_status(
    bool parsed_success,
    bool location_array_available,
    bool location_candidate_available,
    const char *response_code);

constexpr bool qweather_city_lookup_should_try_alternate(
    QweatherCityLookupStatus status)
{
    return status == kQweatherCityLookupNotFound;
}
