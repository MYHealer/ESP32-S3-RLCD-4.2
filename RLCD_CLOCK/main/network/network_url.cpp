// 实现网络请求共用的 URL component 编码。
#include "network_url.h"

#include "app_constexpr.h"
#include "app_text_format.h"

namespace {
constexpr size_t kUrlEncodedPlainCharSize = 1;
constexpr size_t kUrlEncodedEscapedCharSize = 3;
constexpr const char *kUrlHexDigits = "0123456789ABCDEF";

static_assert(kUrlEncodedPlainCharSize == 1,
              "URL unreserved characters encode to one byte");
static_assert(kUrlEncodedEscapedCharSize == 3,
              "URL escaped characters encode as %XX");
static_assert(kUrlEncodedEscapedCharSize == 1 + 2 * kUrlEncodedPlainCharSize,
              "URL escaped characters must reserve percent plus two hex digits");
static_assert(cstr_nonempty(kUrlHexDigits), "URL hex digit table must be non-empty");
static_assert(cstr_length(kUrlHexDigits) == 16,
              "URL hex digit table must contain 16 characters");
} // namespace

bool url_is_unreserved(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
           ch == '.' || ch == '~';
}

bool url_encode_component(const char *in, char *out, size_t out_len)
{
    if (!in || !app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    size_t pos = 0;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(in); *p; ++p) {
        if (url_is_unreserved(static_cast<char>(*p))) {
            if (pos + kUrlEncodedPlainCharSize >= out_len) {
                return false;
            }
            out[pos++] = static_cast<char>(*p);
        } else {
            if (pos + kUrlEncodedEscapedCharSize >= out_len) {
                return false;
            }
            out[pos++] = '%';
            out[pos++] = kUrlHexDigits[*p >> 4];
            out[pos++] = kUrlHexDigits[*p & 0x0F];
        }
    }
    out[pos] = '\0';
    return true;
}
