// 验证 QWeather 专属 API Host 的规范化、旧域名拒绝和 DNS 标签边界。
#include "qweather_api_host.h"

#include <assert.h>
#include <string.h>

int main()
{
    char host[kQweatherApiHostLen] = {};
    assert(normalize_qweather_api_host(
        "  Nv4Ky2QuY7.Re.QWeatherApi.Com  ", host, sizeof(host)));
    assert(strcmp(host, "nv4ky2quy7.re.qweatherapi.com") == 0);
    assert(qweather_api_host_input_valid("abc123.qweatherapi.com"));

    const char *invalid_hosts[] = {
        nullptr,
        "",
        "qweatherapi.com",
        "api.qweather.com",
        "devapi.qweather.com",
        "geoapi.qweather.com",
        "https://abc123.qweatherapi.com",
        "abc123.qweatherapi.com/v7/weather/now",
        "abc123.qweatherapi.com:443",
        ".abc123.qweatherapi.com",
        "abc123..qweatherapi.com",
        "-abc123.qweatherapi.com",
        "abc123-.qweatherapi.com",
        "abc_123.qweatherapi.com",
        "example.com",
    };
    for (const char *invalid : invalid_hosts) {
        strlcpy(host, "stale", sizeof(host));
        assert(!normalize_qweather_api_host(invalid, host, sizeof(host)));
        assert(host[0] == '\0');
    }

    char too_small[8] = "stale";
    assert(!normalize_qweather_api_host(
        "abc123.qweatherapi.com", too_small, sizeof(too_small)));
    assert(too_small[0] == '\0');
    assert(!normalize_qweather_api_host(
        "abc123.qweatherapi.com", nullptr, 0));

    char long_label[96] = {};
    memset(long_label, 'a', 64);
    strlcpy(long_label + 64, ".qweatherapi.com", sizeof(long_label) - 64);
    assert(!qweather_api_host_input_valid(long_label));
    return 0;
}
