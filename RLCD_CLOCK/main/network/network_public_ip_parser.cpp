// 解析网络检测公网 IPv4 的 JSON 或纯文本响应。
#include "network_public_ip_parser.h"

#include "network_json.h"
#include "network_json_root.h"

#include "cJSON.h"

#include <string.h>

namespace {
constexpr const char *kPublicIpJsonKey = "ip";
constexpr int kPublicIpJsonSearchMaxDepth = 8;
constexpr size_t kIpv4TextSize = sizeof("255.255.255.255");

constexpr bool ascii_space(char ch)
{
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

constexpr bool ascii_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

bool ipv4_text_valid(const char *text)
{
    if (!text || text[0] == '\0') {
        return false;
    }
    const char *cursor = text;
    for (int octet = 0; octet < 4; ++octet) {
        if (!ascii_digit(*cursor)) {
            return false;
        }
        unsigned value = 0;
        unsigned digits = 0;
        while (ascii_digit(*cursor)) {
            value = value * 10U + static_cast<unsigned>(*cursor - '0');
            ++digits;
            ++cursor;
            if (digits > 3 || value > 255U) {
                return false;
            }
        }
        if (octet < 3) {
            if (*cursor != '.') {
                return false;
            }
            ++cursor;
        } else if (*cursor != '\0') {
            return false;
        }
    }
    return true;
}

bool copy_ipv4_range(const char *start,
                     size_t len,
                     char *out,
                     size_t out_len)
{
    if (!start || !out || out_len == 0 ||
        len == 0 || len >= kIpv4TextSize || len >= out_len) {
        return false;
    }
    char candidate[kIpv4TextSize] = {};
    memcpy(candidate, start, len);
    candidate[len] = '\0';
    if (!ipv4_text_valid(candidate)) {
        return false;
    }
    memcpy(out, candidate, len + 1);
    return true;
}

bool copy_ipv4_exact(const char *text, char *out, size_t out_len)
{
    if (!text) {
        return false;
    }
    while (ascii_space(*text)) {
        ++text;
    }
    const char *end = text + strlen(text);
    while (end > text && ascii_space(end[-1])) {
        --end;
    }
    return copy_ipv4_range(text,
                           static_cast<size_t>(end - text),
                           out,
                           out_len);
}

bool copy_ipv4_token(const char *text, char *out, size_t out_len)
{
    if (!text) {
        return false;
    }
    while (ascii_space(*text)) {
        ++text;
    }
    size_t len = 0;
    while (text[len] != '\0' && !ascii_space(text[len])) {
        ++len;
    }
    return copy_ipv4_range(text, len, out, out_len);
}

bool find_public_ip_recursive(const cJSON *node,
                              char *out,
                              size_t out_len,
                              int depth)
{
    if (!node || !out || out_len == 0 ||
        depth > kPublicIpJsonSearchMaxDepth) {
        return false;
    }
    if (cJSON_IsObject(node)) {
        const cJSON *item =
            cJSON_GetObjectItemCaseSensitive(node, kPublicIpJsonKey);
        if (const char *value = network_json_string_value(item);
            copy_ipv4_exact(value, out, out_len)) {
            return true;
        }
        cJSON_ArrayForEach(item, node)
        {
            if (find_public_ip_recursive(item, out, out_len, depth + 1)) {
                return true;
            }
        }
    } else if (cJSON_IsArray(node)) {
        const cJSON *item = nullptr;
        cJSON_ArrayForEach(item, node)
        {
            if (find_public_ip_recursive(item, out, out_len, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

bool network_public_ip_parse_response(const char *response,
                                      char *out,
                                      size_t out_len)
{
    if (!response || !out || out_len == 0) {
        return false;
    }
    char candidate[kIpv4TextSize] = {};
    NetworkJsonRoot root(response);
    bool parsed = false;
    if (root) {
        if (const char *root_value = network_json_string_value(root.get())) {
            parsed = copy_ipv4_exact(root_value,
                                     candidate,
                                     sizeof(candidate));
        } else {
            parsed = find_public_ip_recursive(root.get(),
                                              candidate,
                                              sizeof(candidate),
                                              0);
        }
    }
    if (!parsed) {
        parsed = copy_ipv4_token(response, candidate, sizeof(candidate));
    }
    if (!parsed) {
        return false;
    }
    const size_t candidate_len = strlen(candidate);
    if (candidate_len >= out_len) {
        return false;
    }
    memcpy(out, candidate, candidate_len + 1);
    return true;
}
