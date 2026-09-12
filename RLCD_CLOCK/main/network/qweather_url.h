// 声明 QWeather 各 endpoint 的统一 URL 构建接口。
#pragma once

#include <stddef.h>

inline constexpr size_t kQweatherRequestUrlSize = 384;
inline constexpr size_t kQweatherEncodedLocationSize = 128;

enum QweatherUrlStatus {
    kQweatherUrlOk = 0,
    kQweatherUrlInvalidArgument,
    kQweatherUrlLocationTooLong,
    kQweatherUrlTooLong,
};

QweatherUrlStatus build_qweather_city_lookup_url(char *out,
                                                 size_t out_len,
                                                 const char *host,
                                                 const char *location);
QweatherUrlStatus build_qweather_alert_url(char *out,
                                           size_t out_len,
                                           const char *host,
                                           const char *latitude,
                                           const char *longitude);
QweatherUrlStatus build_qweather_now_url(char *out,
                                        size_t out_len,
                                        const char *host,
                                        const char *city_id);
QweatherUrlStatus build_qweather_daily_url(char *out,
                                           size_t out_len,
                                           const char *host,
                                           const char *city_id,
                                           int days);
QweatherUrlStatus build_qweather_air_url(char *out,
                                        size_t out_len,
                                        const char *host,
                                        const char *latitude,
                                        const char *longitude);
