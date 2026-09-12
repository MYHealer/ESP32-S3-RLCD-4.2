// 构建小智 MCP initialize 响应和工具 JSON schema，不执行具体工具业务。
#include "xiaozhi_mcp_schema.h"
#include "xiaozhi_mcp_json.h"

#include <stdint.h>

namespace xiaozhi_mcp_schema {

const char kDeviceStatusTool[] = "self.get_device_status";
const char kSetVolumeTool[] = "self.audio_speaker.set_volume";
const char kSetAlarmTool[] = "self.alarm.set";
const char kDisableAlarmTool[] = "self.alarm.disable";
const char kSetCountdownTool[] = "self.timer.set_countdown";
const char kPomodoroControlTool[] = "self.pomodoro.control";
const char kSetWeatherCityTool[] = "self.weather.set_city";

namespace {
constexpr const char *kMcpProtocolVersion = "2024-11-05";
constexpr const char *kMcpServerName = "ESP32-S3-RLCD-4.2";
constexpr const char *kJsonFieldType = "type";
constexpr const char *kJsonFieldProperties = "properties";
constexpr const char *kJsonFieldRequired = "required";
constexpr const char *kJsonFieldAction = "action";
constexpr const char *kJsonFieldDurationSeconds = "duration_seconds";
constexpr const char *kJsonTypeObject = "object";
constexpr const char *kJsonTypeString = "string";
constexpr const char *kJsonTypeInteger = "integer";
constexpr const char *kJsonTypeBoolean = "boolean";
constexpr const char *kPomodoroActionStart = "start";
constexpr const char *kPomodoroActionCancel = "cancel";
constexpr const char *kPomodoroActionStatus = "status";
constexpr const char *kDeviceStatusDescription =
    "Get local temperature, humidity, battery percentage and speaker volume.";
constexpr const char *kSetVolumeDescription =
    "Set the device speaker volume from 0 to 100 percent.";
constexpr const char *kSetAlarmDescription =
    "Set the one-shot alarm at the next occurrence of the supplied 24-hour local hour and minute. If another alarm already exists at a different time, the first call MUST omit confirm_replace or set it to false. The device then returns confirmation_required with the existing and requested times; ask the user whether to replace it. Only after the user explicitly confirms may you call the same requested time again with confirm_replace=true. Never set confirm_replace=true on the initial request. Setting the same time is idempotent and needs no confirmation.";
constexpr const char *kDisableAlarmDescription =
    "Disable or stop the local alarm.";
constexpr const char *kSetCountdownDescription =
    "Set a local countdown reminder in seconds. Never use this tool for requests containing focus, 专注, or 番茄钟; those requests MUST use self.pomodoro.control.";
constexpr const char *kPomodoroControlDescription =
    "Control the focus Pomodoro timer. You MUST call this tool for every request containing focus, 专注, or 番茄钟, and MUST NOT claim that a timer started or changed unless the tool returned success. "
    "Use start to create or replace it, cancel to stop it, and status to query it. "
    "Do not use alarm tools for focus or Pomodoro requests, and do not use this tool for ordinary reminders.";
constexpr const char *kSetWeatherCityDescription =
    "Set the QWeather location from the user's spoken city name. Pass the city exactly as spoken, including an optional Chinese 市 suffix; the device normalizes and validates a manual city with QWeather before saving. To restore IP-based automatic location, call this same tool with city=自动. Never claim the location changed unless the tool returned success.";
constexpr int kHoursPerDay = 24;
constexpr int kMinutesPerHour = 60;
constexpr uint32_t kMaxCountdownSeconds = 7U * 24U * 60U * 60U;
constexpr uint32_t kMaxPomodoroSeconds = 99U * 60U + 59U;

using xiaozhi_mcp_json::add_string;
using xiaozhi_mcp_json::add_owned_item_to_array;
using xiaozhi_mcp_json::add_owned_item_to_object;

cJSON *create_object_schema()
{
    cJSON *schema = cJSON_CreateObject();
    if (!schema || !add_string(schema, kJsonFieldType, kJsonTypeObject) ||
        !add_owned_item_to_object(schema,
                                  kJsonFieldProperties,
                                  cJSON_CreateObject())) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return schema;
}

cJSON *create_tool(const char *name, const char *description, cJSON *input_schema)
{
    if (!input_schema) {
        return nullptr;
    }
    cJSON *tool = cJSON_CreateObject();
    if (!tool || !add_string(tool, "name", name) ||
        !add_string(tool, "description", description)) {
        cJSON_Delete(tool);
        cJSON_Delete(input_schema);
        return nullptr;
    }
    if (!add_owned_item_to_object(tool, "inputSchema", input_schema)) {
        cJSON_Delete(tool);
        return nullptr;
    }
    return tool;
}

cJSON *get_or_create_required_array(cJSON *schema)
{
    cJSON *required = schema ? cJSON_GetObjectItem(schema, kJsonFieldRequired) : nullptr;
    if (required) {
        return cJSON_IsArray(required) ? required : nullptr;
    }
    required = cJSON_CreateArray();
    if (!add_owned_item_to_object(schema, kJsonFieldRequired, required)) {
        return nullptr;
    }
    return required;
}

bool add_required_integer(cJSON *schema,
                          const char *name,
                          int minimum,
                          int maximum)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    cJSON *required = get_or_create_required_array(schema);
    if (!cJSON_IsObject(properties) || !property || !cJSON_IsArray(required) ||
        !add_string(property, kJsonFieldType, kJsonTypeInteger) ||
        !cJSON_AddNumberToObject(property, "minimum", minimum) ||
        !cJSON_AddNumberToObject(property, "maximum", maximum) ||
        !add_owned_item_to_array(required, cJSON_CreateString(name))) {
        cJSON_Delete(property);
        return false;
    }
    return add_owned_item_to_object(properties, name, property);
}

bool add_optional_string(cJSON *schema, const char *name)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    if (!cJSON_IsObject(properties) || !property ||
        !add_string(property, kJsonFieldType, kJsonTypeString)) {
        cJSON_Delete(property);
        return false;
    }
    return add_owned_item_to_object(properties, name, property);
}

bool add_optional_boolean(cJSON *schema, const char *name)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    if (!cJSON_IsObject(properties) || !property ||
        !add_string(property, kJsonFieldType, kJsonTypeBoolean)) {
        cJSON_Delete(property);
        return false;
    }
    return add_owned_item_to_object(properties, name, property);
}

bool add_required_string(cJSON *schema, const char *name)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    cJSON *required = get_or_create_required_array(schema);
    if (!cJSON_IsObject(properties) || !property || !cJSON_IsArray(required) ||
        !add_string(property, kJsonFieldType, kJsonTypeString) ||
        !add_owned_item_to_array(required, cJSON_CreateString(name))) {
        cJSON_Delete(property);
        return false;
    }
    return add_owned_item_to_object(properties, name, property);
}

bool add_optional_integer(cJSON *schema, const char *name, int minimum, int maximum)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    if (!cJSON_IsObject(properties) || !property ||
        !add_string(property, kJsonFieldType, kJsonTypeInteger) ||
        !cJSON_AddNumberToObject(property, "minimum", minimum) ||
        !cJSON_AddNumberToObject(property, "maximum", maximum)) {
        cJSON_Delete(property);
        return false;
    }
    return add_owned_item_to_object(properties, name, property);
}

bool add_required_action(cJSON *schema)
{
    cJSON *properties = schema ? cJSON_GetObjectItem(schema, kJsonFieldProperties) : nullptr;
    cJSON *property = cJSON_CreateObject();
    cJSON *values = cJSON_CreateArray();
    cJSON *required = cJSON_CreateArray();
    if (!cJSON_IsObject(properties) || !property || !values || !required ||
        !add_string(property, kJsonFieldType, kJsonTypeString) ||
        !add_owned_item_to_array(values, cJSON_CreateString(kPomodoroActionStart)) ||
        !add_owned_item_to_array(values, cJSON_CreateString(kPomodoroActionCancel)) ||
        !add_owned_item_to_array(values, cJSON_CreateString(kPomodoroActionStatus)) ||
        !add_owned_item_to_array(required, cJSON_CreateString(kJsonFieldAction))) {
        cJSON_Delete(property);
        cJSON_Delete(values);
        cJSON_Delete(required);
        return false;
    }
    if (!add_owned_item_to_object(property, "enum", values)) {
        cJSON_Delete(property);
        cJSON_Delete(required);
        return false;
    }
    if (!add_owned_item_to_object(properties, kJsonFieldAction, property)) {
        cJSON_Delete(required);
        return false;
    }
    return add_owned_item_to_object(schema, kJsonFieldRequired, required);
}

cJSON *create_integer_tool(const char *name,
                           const char *description,
                           const char *argument,
                           int minimum,
                           int maximum)
{
    cJSON *schema = create_object_schema();
    if (!schema || !add_required_integer(schema, argument, minimum, maximum)) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return create_tool(name, description, schema);
}

cJSON *create_alarm_tool()
{
    cJSON *schema = create_object_schema();
    if (!schema ||
        !add_required_integer(schema, "hour", 0, kHoursPerDay - 1) ||
        !add_required_integer(schema, "minute", 0, kMinutesPerHour - 1) ||
        !add_optional_boolean(schema, "confirm_replace") ||
        !add_optional_string(schema, "label")) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return create_tool(kSetAlarmTool, kSetAlarmDescription, schema);
}

cJSON *create_countdown_tool()
{
    cJSON *schema = create_object_schema();
    if (!schema ||
        !add_required_integer(schema,
                              kJsonFieldDurationSeconds,
                              1,
                              static_cast<int>(kMaxCountdownSeconds)) ||
        !add_optional_string(schema, "label")) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return create_tool(kSetCountdownTool, kSetCountdownDescription, schema);
}

cJSON *create_pomodoro_tool()
{
    cJSON *schema = create_object_schema();
    if (!schema || !add_required_action(schema) ||
        !add_optional_integer(schema,
                              kJsonFieldDurationSeconds,
                              1,
                              static_cast<int>(kMaxPomodoroSeconds))) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return create_tool(kPomodoroControlTool, kPomodoroControlDescription, schema);
}

cJSON *create_weather_city_tool()
{
    cJSON *schema = create_object_schema();
    if (!schema || !add_required_string(schema, "city")) {
        cJSON_Delete(schema);
        return nullptr;
    }
    return create_tool(kSetWeatherCityTool, kSetWeatherCityDescription, schema);
}

bool add_tool_if_present(cJSON *tools, cJSON *tool)
{
    return add_owned_item_to_array(tools, tool);
}
} // namespace

cJSON *create_tools_list_result(bool alarm_enabled,
                                bool alarm_disable_enabled,
                                bool countdown_enabled,
                                bool pomodoro_enabled,
                                bool weather_city_enabled)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return nullptr;
    }
    cJSON *tools = cJSON_CreateArray();
    if (!tools) {
        cJSON_Delete(result);
        return nullptr;
    }
    if (!add_tool_if_present(tools, create_tool(kDeviceStatusTool,
                                                kDeviceStatusDescription,
                                                create_object_schema())) ||
        !add_tool_if_present(tools, create_integer_tool(kSetVolumeTool,
                                                        kSetVolumeDescription,
                                                        "volume",
                                                        0,
                                                        100)) ||
        (alarm_enabled && !add_tool_if_present(tools, create_alarm_tool())) ||
        (alarm_disable_enabled &&
         !add_tool_if_present(tools, create_tool(kDisableAlarmTool,
                                                 kDisableAlarmDescription,
                                                 create_object_schema()))) ||
        (countdown_enabled && !add_tool_if_present(tools, create_countdown_tool())) ||
        (pomodoro_enabled && !add_tool_if_present(tools, create_pomodoro_tool())) ||
        (weather_city_enabled && !add_tool_if_present(tools, create_weather_city_tool()))) {
        cJSON_Delete(tools);
        cJSON_Delete(result);
        return nullptr;
    }
    if (!add_owned_item_to_object(result, "tools", tools)) {
        cJSON_Delete(result);
        return nullptr;
    }
    return result;
}

cJSON *create_initialize_result(const char *app_version)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return nullptr;
    }
    cJSON *capabilities = cJSON_CreateObject();
    if (!capabilities) {
        cJSON_Delete(result);
        return nullptr;
    }
    cJSON *tools = cJSON_CreateObject();
    if (!tools) {
        cJSON_Delete(capabilities);
        cJSON_Delete(result);
        return nullptr;
    }
    cJSON *server_info = cJSON_CreateObject();
    if (!server_info) {
        cJSON_Delete(tools);
        cJSON_Delete(capabilities);
        cJSON_Delete(result);
        return nullptr;
    }
    if (!add_string(result, "protocolVersion", kMcpProtocolVersion) ||
        !add_string(server_info, "name", kMcpServerName) ||
        !add_string(server_info, "version", app_version)) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(tools);
        cJSON_Delete(server_info);
        return nullptr;
    }
    if (!add_owned_item_to_object(capabilities, "tools", tools)) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(server_info);
        return nullptr;
    }
    if (!add_owned_item_to_object(result, "capabilities", capabilities)) {
        cJSON_Delete(result);
        cJSON_Delete(server_info);
        return nullptr;
    }
    if (!add_owned_item_to_object(result, "serverInfo", server_info)) {
        cJSON_Delete(result);
        return nullptr;
    }
    return result;
}

} // namespace xiaozhi_mcp_schema
