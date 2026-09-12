// 验证配网页文本转义覆盖属性引号、正文特殊字符和小缓冲边界。
#include "wifi_portal_html_text.h"

#include <assert.h>
#include <string.h>

int main()
{
    char escaped[160] = {};
    wifi_portal_html::escape_text(
        "Alice's Wi-Fi & <Home> \"2.4G\" 杭州",
        escaped,
        sizeof(escaped));
    assert(strcmp(escaped,
                  "Alice&#39;s Wi-Fi &amp; &lt;Home&gt; &quot;2.4G&quot; 杭州") == 0);

    wifi_portal_html::escape_text(nullptr, escaped, sizeof(escaped));
    assert(escaped[0] == '\0');

    char exact_entity[6] = {};
    wifi_portal_html::escape_text("&", exact_entity, sizeof(exact_entity));
    assert(strcmp(exact_entity, "&amp;") == 0);

    char short_entity[5] = {'x', 'x', 'x', 'x', '\0'};
    wifi_portal_html::escape_text("&", short_entity, sizeof(short_entity));
    assert(short_entity[0] == '\0');

    char quote_input[33] = {};
    memset(quote_input, '"', sizeof(quote_input) - 1);
    char quote_output[32 * 6 + 1] = {};
    wifi_portal_html::escape_text(
        quote_input, quote_output, sizeof(quote_output));
    assert(strlen(quote_output) == sizeof(quote_output) - 1);
    for (size_t index = 0; index < sizeof(quote_output) - 1; index += 6) {
        assert(memcmp(quote_output + index, "&quot;", 6) == 0);
    }

    wifi_portal_html::escape_text("unchanged", nullptr, 0);
    return 0;
}
