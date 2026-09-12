// 验证 URL component 编码的字符集合、UTF-8 和容量边界。
#include "network_url.h"

#include <assert.h>
#include <string.h>

int main()
{
    const char *unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    for (const char *p = unreserved; *p; ++p) {
        assert(url_is_unreserved(*p));
    }
    assert(!url_is_unreserved(' '));
    assert(!url_is_unreserved('/'));
    assert(!url_is_unreserved('%'));

    char out[128] = {};
    assert(url_encode_component(unreserved, out, sizeof(out)));
    assert(strcmp(out, unreserved) == 0);
    assert(url_encode_component("杭州 A/B%", out, sizeof(out)));
    assert(strcmp(out, "%E6%9D%AD%E5%B7%9E%20A%2FB%25") == 0);

    char empty[1] = {'x'};
    assert(url_encode_component("", empty, sizeof(empty)));
    assert(empty[0] == '\0');

    char exact[4] = {};
    assert(url_encode_component("/", exact, sizeof(exact)));
    assert(strcmp(exact, "%2F") == 0);
    char short_out[3] = {};
    assert(!url_encode_component("/", short_out, sizeof(short_out)));

    assert(!url_encode_component(nullptr, out, sizeof(out)));
    assert(!url_encode_component("text", nullptr, sizeof(out)));
    assert(!url_encode_component("text", out, 0));
    return 0;
}
