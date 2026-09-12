// 解析配网页 application/x-www-form-urlencoded 字段，保持兼容字段和非法转义回退语义。
#include "network_form.h"

#include "app_metadata.h"
#include "app_text_format.h"

#include <esp_log.h>

#include <string.h>

#define FORM_VALUE_TRUNCATED_LOG_FORMAT "form value truncated for key=%s len=%u cap=%u"

namespace {
constexpr char kFormKeyValueSeparator = '=';
constexpr char kFormFieldSeparator = '&';
constexpr char kUrlPercentMarker = '%';
constexpr char kFormEncodedSpace = '+';
constexpr char kFormDecodedSpace = ' ';
constexpr size_t kFormSeparatorLen = 1;
constexpr size_t kUrlPercentMarkerIndex = 0;
constexpr size_t kUrlPercentHighNibbleIndex = 1;
constexpr size_t kUrlPercentLowNibbleIndex = 2;
constexpr size_t kUrlPercentDecodedSkip = kUrlPercentLowNibbleIndex;
constexpr int kHexAlphaDigitBaseValue = 10;
constexpr int kHexHighNibbleShift = 4;

static_assert(kNetworkFormEncodedBufferSize > 0, "form encoded scratch buffer must be nonzero");
static_assert(kFormSeparatorLen == 1, "HTML form separators must be one byte");
static_assert(kUrlPercentMarkerIndex == 0, "Percent-encoded byte must start with marker");
static_assert(kUrlPercentHighNibbleIndex == kUrlPercentMarkerIndex + 1,
              "Percent-encoded high nibble must follow marker");
static_assert(kUrlPercentLowNibbleIndex == kUrlPercentHighNibbleIndex + 1,
              "Percent-encoded low nibble must follow high nibble");
static_assert(kUrlPercentDecodedSkip == 2, "Percent decode must skip two hex characters");
static_assert(kHexAlphaDigitBaseValue == 10, "Hex alpha digits start at decimal 10");
static_assert(kHexHighNibbleShift == 4, "Hex high nibble shift must remain 4 bits");

bool form_field_matches_key(const char *field, size_t field_len, const char *key, size_t key_len)
{
    return field && key &&
           field_len > key_len &&
           field[key_len] == kFormKeyValueSeparator &&
           strncmp(field, key, key_len) == 0;
}

bool find_form_value_range(const char *body, const char *key, const char **value_start, size_t *value_len)
{
    if (!body || !key || key[0] == '\0' || !value_start || !value_len) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char *field = body;
    while (*field) {
        const char *field_end = strchr(field, kFormFieldSeparator);
        const size_t field_len = field_end ? static_cast<size_t>(field_end - field) : strlen(field);
        if (form_field_matches_key(field, field_len, key, key_len)) {
            *value_start = field + key_len + kFormSeparatorLen;
            *value_len = field_len - key_len - kFormSeparatorLen;
            return true;
        }
        if (!field_end) {
            break;
        }
        field = field_end + kFormSeparatorLen;
    }
    return false;
}

int hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + kHexAlphaDigitBaseValue;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + kHexAlphaDigitBaseValue;
    }
    return -1;
}

bool decode_url_percent_byte(const char *src, char *out)
{
    if (!src || !out ||
        src[kUrlPercentMarkerIndex] != kUrlPercentMarker ||
        src[kUrlPercentHighNibbleIndex] == '\0' ||
        src[kUrlPercentLowNibbleIndex] == '\0') {
        return false;
    }
    const int hi = hex_digit_value(src[kUrlPercentHighNibbleIndex]);
    const int lo = hex_digit_value(src[kUrlPercentLowNibbleIndex]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    *out = static_cast<char>((hi << kHexHighNibbleShift) | lo);
    return true;
}
} // namespace

void url_decode(char *dst, size_t dst_len, const char *src)
{
    if (!app_text::output_buffer_available(dst, dst_len)) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 1 < dst_len; ++si) {
        char decoded = '\0';
        if (decode_url_percent_byte(&src[si], &decoded)) {
            dst[di++] = decoded;
            si += kUrlPercentDecodedSkip;
        } else if (src[si] == kFormEncodedSpace) {
            dst[di++] = kFormDecodedSpace;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

void form_value(const char *body, const char *key, char *out, size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    if (!body || !key || key[0] == '\0') {
        out[0] = '\0';
        return;
    }
    const char *start = nullptr;
    size_t len = 0;
    if (!find_form_value_range(body, key, &start, &len)) {
        out[0] = '\0';
        return;
    }
    char encoded[kNetworkFormEncodedBufferSize] = {};
    if (len >= sizeof(encoded)) {
        ESP_LOGW(TAG, FORM_VALUE_TRUNCATED_LOG_FORMAT,
                 key,
                 static_cast<unsigned>(len),
                 static_cast<unsigned>(sizeof(encoded)));
        len = sizeof(encoded) - 1;
    }
    memcpy(encoded, start, len);
    encoded[len] = '\0';
    url_decode(out, out_len, encoded);
}

void form_value_fallback(const char *body,
                         const char *primary_key,
                         const char *fallback_key,
                         char *out,
                         size_t out_len)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    form_value(body, primary_key, out, out_len);
    if (out[0] == '\0' && fallback_key) {
        form_value(body, fallback_key, out, out_len);
    }
}
