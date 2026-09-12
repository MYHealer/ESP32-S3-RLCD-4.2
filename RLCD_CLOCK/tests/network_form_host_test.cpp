// 验证通用表单字段精确匹配、URL 解码、fallback 和截断边界。
#include "network_form.h"

#include <assert.h>
#include <string.h>

int main()
{
    char text[64] = {};
    url_decode(text, sizeof(text), "hello+world%21");
    assert(strcmp(text, "hello world!") == 0);
    url_decode(text, sizeof(text), "%E6%9D%AD%E5%B7%9E");
    assert(strcmp(text, "杭州") == 0);
    url_decode(text, sizeof(text), "%G1%2Z%");
    assert(strcmp(text, "%G1%2Z%") == 0);
    url_decode(text, sizeof(text), nullptr);
    assert(text[0] == '\0');
    url_decode(nullptr, 0, "ignored");

    char short_text[5] = {};
    url_decode(short_text, sizeof(short_text), "abcdef");
    assert(strcmp(short_text, "abcd") == 0);

    form_value("xssid=wrong&ssid=correct&name=clock",
               "ssid",
               text,
               sizeof(text));
    assert(strcmp(text, "correct") == 0);
    form_value("ssid=first&ssid=second", "ssid", text, sizeof(text));
    assert(strcmp(text, "first") == 0);
    form_value("ssid=&pass=value", "ssid", text, sizeof(text));
    assert(text[0] == '\0');
    form_value("ssid=value", "missing", text, sizeof(text));
    assert(text[0] == '\0');
    form_value(nullptr, "ssid", text, sizeof(text));
    assert(text[0] == '\0');
    form_value("ssid=value", nullptr, text, sizeof(text));
    assert(text[0] == '\0');
    form_value("ssid=value", "", text, sizeof(text));
    assert(text[0] == '\0');
    form_value("ssid=value", "ssid", nullptr, 0);

    form_value_fallback("primary=&legacy=fallback",
                        "primary",
                        "legacy",
                        text,
                        sizeof(text));
    assert(strcmp(text, "fallback") == 0);
    form_value_fallback("primary=current&legacy=fallback",
                        "primary",
                        "legacy",
                        text,
                        sizeof(text));
    assert(strcmp(text, "current") == 0);
    form_value_fallback("legacy=fallback",
                        "primary",
                        nullptr,
                        text,
                        sizeof(text));
    assert(text[0] == '\0');

    char long_body[kNetworkFormEncodedBufferSize + 64] = "value=";
    memset(long_body + strlen(long_body),
           'a',
           sizeof(long_body) - strlen(long_body) - 1);
    long_body[sizeof(long_body) - 1] = '\0';
    char long_value[kNetworkFormEncodedBufferSize + 8] = {};
    form_value(long_body, "value", long_value, sizeof(long_value));
    assert(strlen(long_value) == kNetworkFormEncodedBufferSize - 1);
    for (size_t index = 0; index < strlen(long_value); ++index) {
        assert(long_value[index] == 'a');
    }
    return 0;
}
