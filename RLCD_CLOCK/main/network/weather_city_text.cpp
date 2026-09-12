// 以纯文本逻辑统一配网页、上位机和小智 MCP 的天气城市格式。
#include "weather_city_text.h"

#include "ascii_text.h"

#include <cstring>

namespace weather_city_text {
namespace {
constexpr const char *kInvalidCharacters = "&=?#%/\\<>\"'";
constexpr const char *kChineseCitySuffix = "市";
constexpr size_t kChineseCitySuffixBytes = 3;

constexpr bool contains_character(const char *text, char needle)
{
    if (!text) {
        return false;
    }
    for (const char *cursor = text; *cursor; ++cursor) {
        if (*cursor == needle) {
            return true;
        }
    }
    return false;
}

static_assert(contains_character(kInvalidCharacters, '&') &&
                  contains_character(kInvalidCharacters, '=') &&
                  contains_character(kInvalidCharacters, '%') &&
                  contains_character(kInvalidCharacters, '/') &&
                  contains_character(kInvalidCharacters, '<') &&
                  contains_character(kInvalidCharacters, '>') &&
                  contains_character(kInvalidCharacters, '"') &&
                  contains_character(kInvalidCharacters, '\''),
              "weather city reject list must keep form, URL and HTML control characters");

} // namespace

bool input_valid(const char *city, size_t maximum_bytes)
{
    if (!city || maximum_bytes == 0) {
        return city == nullptr;
    }
    size_t len = std::strlen(city);
    if (len >= maximum_bytes) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(city[i]);
        if (ch < 0x20 || std::strchr(kInvalidCharacters, ch)) {
            return false;
        }
    }
    return true;
}

bool normalize(const char *city, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!city) {
        return true;
    }
    if (std::strlen(city) >= out_len) {
        return false;
    }
    std::strcpy(out, city);
    trim_ascii_line_whitespace(out);
    size_t len = std::strlen(out);
    if (len > kChineseCitySuffixBytes &&
        std::memcmp(out + len - kChineseCitySuffixBytes,
                    kChineseCitySuffix,
                    kChineseCitySuffixBytes) == 0) {
        out[len - kChineseCitySuffixBytes] = '\0';
        trim_ascii_line_whitespace(out);
    }
    return input_valid(out, out_len);
}
} // namespace weather_city_text
