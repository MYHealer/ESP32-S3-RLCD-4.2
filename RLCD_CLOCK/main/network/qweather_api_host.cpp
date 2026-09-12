// 规范化 QWeather 专属 API Host，并拒绝旧公共域名和非主机名输入。
#include "qweather_api_host.h"

#include <ctype.h>
#include <string.h>

namespace {
constexpr const char *kQweatherApiHostSuffix = ".qweatherapi.com";
constexpr size_t kMaximumDnsLabelLength = 63;

bool dns_label_valid(const char *begin, const char *end)
{
    if (!begin || !end || begin >= end ||
        static_cast<size_t>(end - begin) > kMaximumDnsLabelLength ||
        *begin == '-' || *(end - 1) == '-') {
        return false;
    }
    for (const char *cursor = begin; cursor < end; ++cursor) {
        const unsigned char value = static_cast<unsigned char>(*cursor);
        if (!isalnum(value) && value != '-') {
            return false;
        }
    }
    return true;
}

bool normalized_host_valid(const char *host, size_t host_len)
{
    const size_t suffix_len = strlen(kQweatherApiHostSuffix);
    if (!host || host_len <= suffix_len ||
        strcmp(host + host_len - suffix_len, kQweatherApiHostSuffix) != 0) {
        return false;
    }
    const char *label_begin = host;
    const char *host_end = host + host_len;
    for (const char *cursor = host; cursor <= host_end; ++cursor) {
        if (cursor == host_end || *cursor == '.') {
            if (!dns_label_valid(label_begin, cursor)) {
                return false;
            }
            label_begin = cursor + 1;
        }
    }
    return true;
}
} // namespace

bool normalize_qweather_api_host(const char *input,
                                 char *out,
                                 size_t out_len)
{
    if (!input || !out || out_len == 0) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return false;
    }
    const char *begin = input;
    while (*begin != '\0' &&
           isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    const char *end = begin + strlen(begin);
    while (end > begin &&
           isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    const size_t length = static_cast<size_t>(end - begin);
    if (length == 0 || length >= kQweatherApiHostLen || length >= out_len) {
        out[0] = '\0';
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        out[i] = static_cast<char>(
            tolower(static_cast<unsigned char>(begin[i])));
    }
    out[length] = '\0';
    if (!normalized_host_valid(out, length)) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool qweather_api_host_input_valid(const char *input)
{
    char normalized[kQweatherApiHostLen] = {};
    return normalize_qweather_api_host(input,
                                       normalized,
                                       sizeof(normalized));
}
