// 实现小智 MCP 工具参数的类型、范围和文本边界校验。
#include "xiaozhi_mcp_arguments.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr const char *kJsonFieldAction = "action";
constexpr const char *kJsonFieldDurationSeconds = "duration_seconds";
constexpr const char *kPomodoroActionStart = "start";
constexpr const char *kPomodoroActionCancel = "cancel";
constexpr const char *kPomodoroActionStatus = "status";
constexpr int kHoursPerDay = 24;
constexpr int kMinutesPerHour = 60;
constexpr uint32_t kMaxCountdownSeconds = 7U * 24U * 60U * 60U;
constexpr uint32_t kDefaultPomodoroSeconds = 25U * 60U;
constexpr uint32_t kMaxPomodoroSeconds = 99U * 60U + 59U;

bool json_integer_in_range(const cJSON *object,
                           const char *name,
                           int minimum,
                           int maximum,
                           int *value)
{
    if (!object || !name || !cJSON_IsObject(object)) {
        return false;
    }
    const cJSON *item = cJSON_GetObjectItem(object, name);
    if (!item || !cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        item->valuedouble != static_cast<double>(item->valueint) ||
        item->valueint < minimum || item->valueint > maximum) {
        return false;
    }
    if (value) {
        *value = item->valueint;
    }
    return true;
}

void copy_optional_label(const cJSON *arguments, char *label, size_t label_len)
{
    if (!label || label_len == 0) {
        return;
    }
    label[0] = '\0';
    if (!arguments || !cJSON_IsObject(arguments)) {
        return;
    }
    const cJSON *item = cJSON_GetObjectItem(arguments, "label");
    if (!item || !cJSON_IsString(item) || !item->valuestring) {
        return;
    }
    const size_t source_len = std::strlen(item->valuestring);
    const size_t copy_len = std::min(source_len, label_len - 1);
    std::memcpy(label, item->valuestring, copy_len);
    label[copy_len] = '\0';
}
} // namespace

namespace xiaozhi_mcp_arguments {
bool parse_volume(const cJSON *arguments, int *volume)
{
    return volume && json_integer_in_range(arguments, "volume", 0, 100, volume);
}

bool parse_alarm(const cJSON *arguments, XiaozhiMcpAlarmRequest *request)
{
    if (!request) {
        return false;
    }
    XiaozhiMcpAlarmRequest parsed = {};
    if (!json_integer_in_range(arguments, "hour", 0, kHoursPerDay - 1, &parsed.hour) ||
        !json_integer_in_range(arguments, "minute", 0, kMinutesPerHour - 1, &parsed.minute)) {
        return false;
    }
    const cJSON *confirm_replace = cJSON_GetObjectItem(arguments, "confirm_replace");
    if (confirm_replace && !cJSON_IsBool(confirm_replace)) {
        return false;
    }
    parsed.confirm_replace = cJSON_IsTrue(confirm_replace);
    copy_optional_label(arguments, parsed.label, sizeof(parsed.label));
    *request = parsed;
    return true;
}

bool parse_countdown(const cJSON *arguments, XiaozhiMcpCountdownRequest *request)
{
    if (!request) {
        return false;
    }
    int seconds = 0;
    if (!json_integer_in_range(arguments,
                               kJsonFieldDurationSeconds,
                               1,
                               static_cast<int>(kMaxCountdownSeconds),
                               &seconds)) {
        return false;
    }
    XiaozhiMcpCountdownRequest parsed = {};
    parsed.duration_seconds = static_cast<uint32_t>(seconds);
    copy_optional_label(arguments, parsed.label, sizeof(parsed.label));
    *request = parsed;
    return true;
}

bool parse_pomodoro(const cJSON *arguments, XiaozhiMcpPomodoroRequest *request)
{
    if (!request || !arguments || !cJSON_IsObject(arguments)) {
        return false;
    }
    const cJSON *action = cJSON_GetObjectItem(arguments, kJsonFieldAction);
    const cJSON *duration = cJSON_GetObjectItem(arguments, kJsonFieldDurationSeconds);
    if (!action || !cJSON_IsString(action) || !action->valuestring ||
        (duration && !cJSON_IsNumber(duration))) {
        return false;
    }

    XiaozhiMcpPomodoroRequest parsed = {};
    if (std::strcmp(action->valuestring, kPomodoroActionStart) == 0) {
        parsed.action = kXiaozhiMcpPomodoroStart;
        parsed.has_duration_seconds = duration != nullptr;
        parsed.duration_seconds = parsed.has_duration_seconds
                                      ? static_cast<uint32_t>(duration->valueint)
                                      : kDefaultPomodoroSeconds;
        if (parsed.duration_seconds == 0 || parsed.duration_seconds > kMaxPomodoroSeconds ||
            (duration && duration->valuedouble != duration->valueint)) {
            return false;
        }
    } else if (std::strcmp(action->valuestring, kPomodoroActionCancel) == 0) {
        if (duration) {
            return false;
        }
        parsed.action = kXiaozhiMcpPomodoroCancel;
    } else if (std::strcmp(action->valuestring, kPomodoroActionStatus) == 0) {
        if (duration) {
            return false;
        }
        parsed.action = kXiaozhiMcpPomodoroStatus;
    } else {
        return false;
    }
    *request = parsed;
    return true;
}

bool parse_weather_city(const cJSON *arguments, XiaozhiMcpWeatherCityRequest *request)
{
    if (!request || !arguments || !cJSON_IsObject(arguments)) {
        return false;
    }
    const cJSON *city = cJSON_GetObjectItem(arguments, "city");
    if (!city || !cJSON_IsString(city) || !city->valuestring || city->valuestring[0] == '\0' ||
        std::strlen(city->valuestring) >= sizeof(request->city)) {
        return false;
    }
    XiaozhiMcpWeatherCityRequest parsed = {};
    const size_t city_len = std::strlen(city->valuestring);
    std::memcpy(parsed.city, city->valuestring, city_len + 1);
    *request = parsed;
    return true;
}
} // namespace xiaozhi_mcp_arguments
