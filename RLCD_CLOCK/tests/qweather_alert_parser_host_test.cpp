// 验证 QWeather 单条预警嵌套字段、标题回退和颜色优先级解析。
#include "qweather_alert_parser.h"
#include "qweather_alert_text.h"

#include "cJSON.h"

#include <assert.h>
#include <string.h>

namespace {
cJSON *parse_json(const char *text)
{
    cJSON *root = cJSON_Parse(text);
    assert(root != nullptr);
    return root;
}
} // namespace

int main()
{
    QweatherAlertItem parsed = {};
    strlcpy(parsed.title, "unchanged", sizeof(parsed.title));
    parsed.rank = 99;
    assert(!parse_qweather_alert_item(nullptr, &parsed));
    assert(strcmp(parsed.title, "unchanged") == 0);
    assert(parsed.rank == 99);

    cJSON *item = parse_json("[]");
    assert(!parse_qweather_alert_item(item, &parsed));
    assert(strcmp(parsed.title, "unchanged") == 0);
    assert(parsed.rank == 99);
    cJSON_Delete(item);

    item = parse_json(
        "{\"eventType\":{\"name\":\"大风\"},\"color\":{\"code\":\"yellow\"},\"headline\":\"备用标题\"}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(parsed.title_format_ok);
    assert(strcmp(parsed.title, "大风黄色预警") == 0);
    assert(parsed.rank == warning_color_rank("yellow"));
    cJSON_Delete(item);

    item = parse_json(
        "{\"eventType\":{\"name\":\"雷暴\"},\"color\":{\"code\":\"unknown\"},\"headline\":\"雷暴预警标题\"}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(parsed.title_format_ok);
    assert(strcmp(parsed.title, "雷暴预警标题") == 0);
    assert(parsed.rank == 0);
    cJSON_Delete(item);

    item = parse_json("{\"eventType\":{\"name\":\"高温\"}}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(parsed.title_format_ok);
    assert(strcmp(parsed.title, "高温预警") == 0);
    assert(parsed.rank == 0);
    cJSON_Delete(item);

    item = parse_json("{\"headline\":\"道路结冰预警\"}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(parsed.title_format_ok);
    assert(strcmp(parsed.title, "道路结冰预警") == 0);
    assert(parsed.rank == 0);
    cJSON_Delete(item);

    item = parse_json("{}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(parsed.title_format_ok);
    assert(parsed.title[0] == '\0');
    assert(parsed.rank == 0);
    cJSON_Delete(item);

    item = parse_json(
        "{\"EVENTTYPE\":{\"NAME\":\"暴雨\"},\"COLOR\":{\"CODE\":\"red\"}}");
    assert(parse_qweather_alert_item(item, &parsed));
    assert(strcmp(parsed.title, "暴雨红色预警") == 0);
    assert(parsed.rank == warning_color_rank("red"));
    cJSON_Delete(item);

    item = parse_json("{}");
    assert(!parse_qweather_alert_item(item, nullptr));
    cJSON_Delete(item);

    return 0;
}
