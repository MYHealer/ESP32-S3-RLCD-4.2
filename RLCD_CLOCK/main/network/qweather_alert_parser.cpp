// 解析 QWeather 单条预警字段并生成标题，不处理 HTTP、日志或缓存写入。
#include "qweather_alert_parser.h"

#include "network_json.h"
#include "qweather_alert_text.h"

namespace {
constexpr size_t kAlertEventNameSize = 24;
constexpr size_t kAlertColorCodeSize = 16;
constexpr size_t kAlertHeadlineSize = 64;
constexpr const char *kAlertEventTypeField = "eventType";
constexpr const char *kAlertColorField = "color";
constexpr const char *kAlertEventNameField = "name";
constexpr const char *kAlertColorCodeField = "code";
constexpr const char *kAlertHeadlineField = "headline";

static_assert(kAlertEventNameSize > 1, "alert event name buffer must fit text and NUL");
static_assert(kAlertColorCodeSize > 1, "alert color code buffer must fit text and NUL");
static_assert(kAlertHeadlineSize <= kWeatherAlertTitleLen,
              "temporary alert headline must fit final alert title storage");
} // namespace

bool parse_qweather_alert_item(const cJSON *item, QweatherAlertItem *parsed)
{
    if (!cJSON_IsObject(item) || !parsed) {
        return false;
    }

    char event_name[kAlertEventNameSize] = {};
    char color_code[kAlertColorCodeSize] = {};
    char headline[kAlertHeadlineSize] = {};
    const cJSON *event = cJSON_GetObjectItem(item, kAlertEventTypeField);
    const cJSON *color = cJSON_GetObjectItem(item, kAlertColorField);
    if (event) {
        json_copy_string(event, kAlertEventNameField, event_name, sizeof(event_name));
    }
    if (color) {
        json_copy_string(color, kAlertColorCodeField, color_code, sizeof(color_code));
    }
    json_copy_string(item, kAlertHeadlineField, headline, sizeof(headline));

    QweatherAlertItem next = {};
    next.rank = warning_color_rank(color_code);
    next.title_format_ok = build_weather_alert_title(next.title,
                                                     sizeof(next.title),
                                                     event_name,
                                                     color_code,
                                                     headline);
    *parsed = next;
    return true;
}
