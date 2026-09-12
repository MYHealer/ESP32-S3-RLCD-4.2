// 验证配网页字段别名、URL 解码、优先级和 trim 规则。
#include "provisioning_form_fields.h"

#include <assert.h>
#include <string.h>

int main()
{
    ProvisioningFormFields fields = {};
    read_provisioning_form_fields(
        "ssid=My+WiFi&pass=abc%20123&backup_ssid=Mobile+Hotspot&backup_pass=backup%20456&api_key=++key++&api_host=++ABC123.RE.QWEATHERAPI.COM++&weather_city=%E6%9D%AD%E5%B7%9E",
        &fields);
    assert(strcmp(fields.ssid, "My WiFi") == 0);
    assert(strcmp(fields.pass, "abc 123") == 0);
    assert(strcmp(fields.backup_ssid, "Mobile Hotspot") == 0);
    assert(strcmp(fields.backup_pass, "backup 456") == 0);
    assert(strcmp(fields.api_key, "key") == 0);
    assert(strcmp(fields.api_host, "ABC123.RE.QWEATHERAPI.COM") == 0);
    assert(strcmp(fields.weather_city, "杭州") == 0);

    fields = {};
    read_provisioning_form_fields(
        "ssid=+Office+&password=fallback&weather=+legacy-key+&weather_host=+legacy.qweatherapi.com+&city=+%E4%B8%8A%E6%B5%B7+",
        &fields);
    assert(strcmp(fields.ssid, " Office ") == 0);
    assert(strcmp(fields.pass, "fallback") == 0);
    assert(strcmp(fields.api_key, "legacy-key") == 0);
    assert(strcmp(fields.api_host, "legacy.qweatherapi.com") == 0);
    assert(strcmp(fields.weather_city, "上海") == 0);

    fields = {};
    read_provisioning_form_fields(
        "ssid=main&pass=primary&password=fallback&api_key=current&weather=legacy&api_host=current.qweatherapi.com&weather_host=legacy.qweatherapi.com&weather_city=Beijing&city=Shanghai",
        &fields);
    assert(strcmp(fields.pass, "primary") == 0);
    assert(strcmp(fields.api_key, "current") == 0);
    assert(strcmp(fields.api_host, "current.qweatherapi.com") == 0);
    assert(strcmp(fields.weather_city, "Beijing") == 0);

    char manual_time[kProvisioningManualTimeFieldSize] = {};
    read_provisioning_manual_time("manual_time=+2026-07-13+12%3A34+",
                                  manual_time,
                                  sizeof(manual_time));
    assert(strcmp(manual_time, "2026-07-13 12:34") == 0);
    read_provisioning_manual_time("datetime=2026-07-14T05%3A06%3A07",
                                  manual_time,
                                  sizeof(manual_time));
    assert(strcmp(manual_time, "2026-07-14T05:06:07") == 0);

    read_provisioning_form_fields(nullptr, &fields);
    assert(fields.ssid[0] == '\0');
    read_provisioning_form_fields("ssid=test", nullptr);
    read_provisioning_manual_time(nullptr, manual_time, sizeof(manual_time));
    assert(manual_time[0] == '\0');
    read_provisioning_manual_time("manual_time=test", nullptr, 0);
    return 0;
}
