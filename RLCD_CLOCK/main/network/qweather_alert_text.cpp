// 处理 QWeather 预警标题组装、颜色映射、UTF-8 安全压缩和优先级排序。
#include "qweather_alert_text.h"

#include "app_constexpr.h"
#include "app_text_format.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr size_t kWeatherAlertCompactTitleChars = 6;
constexpr unsigned char kUtf8AsciiMask = 0x80;
constexpr unsigned char kUtf8TwoByteMask = 0xE0;
constexpr unsigned char kUtf8TwoBytePrefix = 0xC0;
constexpr unsigned char kUtf8ThreeByteMask = 0xF0;
constexpr unsigned char kUtf8ThreeBytePrefix = 0xE0;
constexpr unsigned char kUtf8FourByteMask = 0xF8;
constexpr unsigned char kUtf8FourBytePrefix = 0xF0;
constexpr unsigned char kUtf8ContinuationMask = 0xC0;
constexpr unsigned char kUtf8ContinuationPrefix = 0x80;
constexpr size_t kUtf8OneByteLen = 1;
constexpr size_t kUtf8TwoByteLen = 2;
constexpr size_t kUtf8ThreeByteLen = 3;
constexpr size_t kUtf8FourByteLen = 4;
constexpr const char *kWeatherAlertEventColorFormat = "%s%s%s";
constexpr const char *kWeatherAlertEventOnlyFormat = "%s%s";

struct WarningColorInfo {
    const char *code;
    const char *name;
    const char *short_name;
    int rank;
};

constexpr WarningColorInfo kWarningColors[] = {
    {"blue", "蓝色", "蓝", 2},
    {"yellow", "黄色", "黄", 3},
    {"orange", "橙色", "橙", 4},
    {"red", "红色", "红", 5},
    {"white", "白色", "白", 1},
    {"black", "黑色", "黑", 1},
};

constexpr bool warning_color_table_valid()
{
    for (const WarningColorInfo &color : kWarningColors) {
        if (!cstr_nonempty(color.code) ||
            !cstr_nonempty(color.name) ||
            !cstr_nonempty(color.short_name) ||
            color.rank < 0) {
            return false;
        }
    }
    return true;
}

const WarningColorInfo *find_warning_color(const char *code)
{
    if (!code) {
        return nullptr;
    }
    for (const WarningColorInfo &color : kWarningColors) {
        if (strcmp(code, color.code) == 0) {
            return &color;
        }
    }
    return nullptr;
}

size_t alert_utf8_char_len(const unsigned char *text)
{
    if (!text || text[0] == '\0') {
        return 0;
    }
    unsigned char ch = text[0];
    size_t expected_len = kUtf8OneByteLen;
    if ((ch & kUtf8AsciiMask) == 0) {
        expected_len = kUtf8OneByteLen;
    } else if ((ch & kUtf8TwoByteMask) == kUtf8TwoBytePrefix) {
        expected_len = kUtf8TwoByteLen;
    } else if ((ch & kUtf8ThreeByteMask) == kUtf8ThreeBytePrefix) {
        expected_len = kUtf8ThreeByteLen;
    } else if ((ch & kUtf8FourByteMask) == kUtf8FourBytePrefix) {
        expected_len = kUtf8FourByteLen;
    }
    for (size_t index = 1; index < expected_len; ++index) {
        if (text[index] == '\0' ||
            (text[index] & kUtf8ContinuationMask) != kUtf8ContinuationPrefix) {
            // 异常外部文本按单字节前进，避免越过结尾或吞掉后续字符。
            return kUtf8OneByteLen;
        }
    }
    return expected_len;
}

size_t alert_utf8_char_count(const char *text)
{
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; p && *p;) {
        size_t len = alert_utf8_char_len(p);
        if (len == 0) {
            break;
        }
        p += len;
        ++count;
    }
    return count;
}

void alert_utf8_copy_chars(char *out, size_t out_len, const char *in, size_t max_chars)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    out[0] = '\0';
    if (!in) {
        return;
    }
    size_t used = 0;
    size_t chars = 0;
    const unsigned char *p = (const unsigned char *)in;
    while (*p && chars < max_chars) {
        size_t len = alert_utf8_char_len(p);
        if (used + len >= out_len) {
            break;
        }
        memcpy(out + used, p, len);
        used += len;
        p += len;
        ++chars;
    }
    out[used] = '\0';
}

void replace_all(char *text, size_t text_len, const char *from, const char *to)
{
    if (!app_text::output_buffer_available(text, text_len) || !from || !to) {
        return;
    }
    char buffer[kWeatherAlertTitleLen] = {};
    const char *read = text;
    size_t used = 0;
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    if (from_len == 0) {
        return;
    }
    while (*read && used + 1 < sizeof(buffer)) {
        if (strncmp(read, from, from_len) == 0) {
            if (used + to_len >= sizeof(buffer)) {
                break;
            }
            memcpy(buffer + used, to, to_len);
            used += to_len;
            read += from_len;
        } else {
            size_t len = alert_utf8_char_len((const unsigned char *)read);
            if (used + len >= sizeof(buffer)) {
                break;
            }
            memcpy(buffer + used, read, len);
            used += len;
            read += len;
        }
    }
    buffer[used] = '\0';
    strlcpy(text, buffer, text_len);
}

void compact_weather_alert_title(char *title, size_t title_len)
{
    if (!title || title[0] == '\0') {
        return;
    }
    if (alert_utf8_char_count(title) <= kWeatherAlertCompactTitleChars) {
        return;
    }
    replace_all(title, title_len, kWeatherAlertSuffix, "");
    for (const WarningColorInfo &color : kWarningColors) {
        replace_all(title, title_len, color.name, color.short_name);
    }
    if (alert_utf8_char_count(title) > kWeatherAlertCompactTitleChars) {
        char clipped[kWeatherAlertTitleLen] = {};
        alert_utf8_copy_chars(clipped, sizeof(clipped), title, kWeatherAlertCompactTitleChars);
        strlcpy(title, clipped, title_len);
    }
}

void copy_compact_weather_alert_title(char *out, size_t out_len, const char *title)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    out[0] = '\0';
    if (!title || title[0] == '\0') {
        return;
    }
    strlcpy(out, title, out_len);
    compact_weather_alert_title(out, out_len);
}

bool format_weather_alert_title(char *title,
                                size_t title_len,
                                const char *format,
                                const char *event_name,
                                const char *color_name = nullptr)
{
    if (!app_text::output_buffer_available(title, title_len) || !format) {
        return false;
    }
    int written = color_name
                      ? snprintf(title, title_len, format, event_name, color_name, kWeatherAlertSuffix)
                      : snprintf(title, title_len, format, event_name, kWeatherAlertSuffix);
    if (written < 0) {
        title[0] = '\0';
        return false;
    }
    return true;
}

static_assert(kWeatherAlertCompactTitleChars > 0,
              "compact weather alert title length must be positive");
static_assert(kUtf8OneByteLen < kUtf8TwoByteLen &&
                  kUtf8TwoByteLen < kUtf8ThreeByteLen &&
                  kUtf8ThreeByteLen < kUtf8FourByteLen,
              "UTF-8 byte length constants must stay ordered");
static_assert((kUtf8ContinuationPrefix & kUtf8ContinuationMask) == kUtf8ContinuationPrefix,
              "UTF-8 continuation prefix must fit its mask");
static_assert(array_count(kWarningColors) > 0, "weather warning color table must not be empty");
static_assert(warning_color_table_valid(), "weather warning color entries must be complete");
} // namespace

const char *warning_color_name(const char *code)
{
    const WarningColorInfo *color = find_warning_color(code);
    return color ? color->name : "";
}

int warning_color_rank(const char *code)
{
    const WarningColorInfo *color = find_warning_color(code);
    return color ? color->rank : 0;
}

bool build_weather_alert_title(char *title,
                               size_t title_len,
                               const char *event_name,
                               const char *color_code,
                               const char *headline)
{
    if (!app_text::output_buffer_available(title, title_len)) {
        return false;
    }
    title[0] = '\0';
    const char *color_name = warning_color_name(color_code);
    if (cstr_nonempty(event_name) && cstr_nonempty(color_name)) {
        return format_weather_alert_title(title,
                                          title_len,
                                          kWeatherAlertEventColorFormat,
                                          event_name,
                                          color_name);
    }
    if (cstr_nonempty(headline)) {
        strlcpy(title, headline, title_len);
        return true;
    }
    if (cstr_nonempty(event_name)) {
        return format_weather_alert_title(title,
                                          title_len,
                                          kWeatherAlertEventOnlyFormat,
                                          event_name);
    }
    return true;
}

void add_weather_alert_title(WeatherAlertData *alert, const char *title, int rank)
{
    if (!alert || !title || title[0] == '\0') {
        return;
    }

    int insert_at = alert->count;
    for (int i = 0; i < alert->count; ++i) {
        if (rank > alert->ranks[i]) {
            insert_at = i;
            break;
        }
    }

    if (alert->count < kMaxWeatherAlerts) {
        ++alert->count;
    } else if (insert_at >= kMaxWeatherAlerts) {
        return;
    }

    for (int i = alert->count - 1; i > insert_at; --i) {
        strlcpy(alert->titles[i], alert->titles[i - 1], sizeof(alert->titles[i]));
        alert->ranks[i] = alert->ranks[i - 1];
    }
    copy_compact_weather_alert_title(alert->titles[insert_at], sizeof(alert->titles[insert_at]), title);
    alert->ranks[insert_at] = rank;
    alert->active = alert->count > 0;
}
