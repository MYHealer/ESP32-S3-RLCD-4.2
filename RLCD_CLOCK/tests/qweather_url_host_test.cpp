// 验证 QWeather 各 endpoint URL、位置编码和容量错误分类。
#include "qweather_url.h"

#include <assert.h>
#include <string.h>

int main()
{
    static_assert(kQweatherRequestUrlSize == 384,
                  "QWeather request URL workspace contract changed");
    constexpr const char *host = "nv4ky2quy7.re.qweatherapi.com";

    char url[kQweatherRequestUrlSize] = {};
    assert(build_qweather_city_lookup_url(url, sizeof(url), host, "杭州") ==
           kQweatherUrlOk);
    assert(strcmp(url,
                  "https://nv4ky2quy7.re.qweatherapi.com/geo/v2/city/lookup?location=%E6%9D%AD%E5%B7%9E&number=1&range=cn&lang=zh") == 0);

    assert(build_qweather_alert_url(
               url, sizeof(url), host, "30.2875", "120.1536") ==
           kQweatherUrlOk);
    assert(strcmp(url,
                  "https://nv4ky2quy7.re.qweatherapi.com/weatheralert/v1/current/30.2875/120.1536?lang=zh&localTime=true") == 0);

    assert(build_qweather_now_url(url, sizeof(url), host, "101210101") ==
           kQweatherUrlOk);
    assert(strcmp(url,
                  "https://nv4ky2quy7.re.qweatherapi.com/v7/weather/now?location=101210101&lang=zh&unit=m") == 0);

    assert(build_qweather_daily_url(url, sizeof(url), host, "101210101", 7) ==
           kQweatherUrlOk);
    assert(strcmp(url,
                  "https://nv4ky2quy7.re.qweatherapi.com/v7/weather/7d?location=101210101&lang=zh&unit=m") == 0);

    assert(build_qweather_air_url(
               url, sizeof(url), host, "30.2875", "120.1536") ==
           kQweatherUrlOk);
    assert(strcmp(url,
                  "https://nv4ky2quy7.re.qweatherapi.com/airquality/v1/current/30.2875/120.1536?lang=zh") == 0);

    assert(build_qweather_air_url(url, sizeof(url), host, "1", "2") ==
           kQweatherUrlOk);
    size_t required_url_size = strlen(url) + 1;
    char exact[128] = {};
    assert(build_qweather_air_url(
               exact, required_url_size, host, "1", "2") ==
           kQweatherUrlOk);
    char short_url[128] = "old";
    assert(build_qweather_air_url(
               short_url, required_url_size - 1, host, "1", "2") ==
           kQweatherUrlTooLong);
    assert(short_url[0] == '\0');

    char long_location[64] = {};
    memset(long_location, '/', sizeof(long_location) - 1);
    strlcpy(url, "unchanged", sizeof(url));
    assert(build_qweather_city_lookup_url(
               url, sizeof(url), host, long_location) ==
           kQweatherUrlLocationTooLong);
    assert(strcmp(url, "unchanged") == 0);

    assert(build_qweather_now_url(url, sizeof(url), host, nullptr) ==
           kQweatherUrlInvalidArgument);
    assert(build_qweather_now_url(url, sizeof(url), nullptr, "101210101") ==
           kQweatherUrlInvalidArgument);
    assert(build_qweather_alert_url(url, sizeof(url), host, nullptr, "120") ==
           kQweatherUrlInvalidArgument);
    assert(build_qweather_air_url(
               nullptr, sizeof(url), host, "30", "120") ==
           kQweatherUrlInvalidArgument);
    assert(build_qweather_air_url(url, 0, host, "30", "120") ==
           kQweatherUrlInvalidArgument);

    char max_encoded_location[43] = {};
    memset(max_encoded_location, '/', sizeof(max_encoded_location) - 1);
    assert(build_qweather_city_lookup_url(
               url, sizeof(url), host, max_encoded_location) ==
           kQweatherUrlOk);
    assert(build_qweather_now_url(url,
                                  sizeof(url),
                                  host,
                                  max_encoded_location) == kQweatherUrlOk);
    assert(build_qweather_daily_url(url,
                                    sizeof(url),
                                    host,
                                    max_encoded_location,
                                    7) == kQweatherUrlOk);
    assert(build_qweather_air_url(url,
                                  sizeof(url),
                                  host,
                                  "30.2875",
                                  "120.1536") == kQweatherUrlOk);
    return 0;
}
