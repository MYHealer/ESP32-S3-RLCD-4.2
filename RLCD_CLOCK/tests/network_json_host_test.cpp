// 验证网络 JSON 字符串复制的参数、兼容匹配和输出边界。
#include "network_json.h"

#include <assert.h>
#include <string.h>

int main()
{
    cJSON *root = cJSON_Parse(R"({"Name":"杭州","count":3,"empty":null})");
    assert(root != nullptr);

    const cJSON *name = cJSON_GetObjectItem(root, "Name");
    assert(strcmp(network_json_string_value(name), "杭州") == 0);
    assert(strcmp(network_json_object_string_value(root, "name"), "杭州") == 0);
    assert(network_json_string_value(nullptr) == nullptr);
    assert(network_json_string_value(cJSON_GetObjectItem(root, "count")) == nullptr);
    assert(network_json_object_string_value(nullptr, "Name") == nullptr);
    assert(network_json_object_string_value(root, nullptr) == nullptr);
    assert(network_json_object_string_value(root, "missing") == nullptr);

    char out[16] = "keep";
    assert(json_copy_string(root, "name", out, sizeof(out)));
    assert(strcmp(out, "杭州") == 0);

    char short_out[4] = {};
    assert(json_copy_string(root, "Name", short_out, sizeof(short_out)));
    assert(short_out[sizeof(short_out) - 1] == '\0');

    char exact_out[16] = {};
    assert(json_copy_string_exact(root, "Name", exact_out, sizeof(exact_out)));
    assert(strcmp(exact_out, "杭州") == 0);

    char exact_short_out[4] = "old";
    assert(!json_copy_string_exact(root,
                                   "Name",
                                   exact_short_out,
                                   sizeof(exact_short_out)));
    assert(strcmp(exact_short_out, "old") == 0);

    strlcpy(out, "keep", sizeof(out));
    assert(!json_copy_string(root, "missing", out, sizeof(out)));
    assert(strcmp(out, "keep") == 0);
    assert(!json_copy_string(root, "count", out, sizeof(out)));
    assert(strcmp(out, "keep") == 0);
    assert(!json_copy_string(nullptr, "Name", out, sizeof(out)));
    assert(!json_copy_string(root, nullptr, out, sizeof(out)));
    assert(!json_copy_string(root, "Name", nullptr, sizeof(out)));
    assert(!json_copy_string(root, "Name", out, 0));
    assert(!json_copy_string_exact(root, "missing", out, sizeof(out)));
    assert(!json_copy_string_exact(nullptr, "Name", out, sizeof(out)));
    assert(!json_copy_string_exact(root, nullptr, out, sizeof(out)));
    assert(!json_copy_string_exact(root, "Name", nullptr, sizeof(out)));
    assert(!json_copy_string_exact(root, "Name", out, 0));

    cJSON_Delete(root);
    return 0;
}
