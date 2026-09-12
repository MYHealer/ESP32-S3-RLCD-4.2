// 在主机侧验证小智 MCP 工具白名单、响应结构和预留 handler 行为。
#include "xiaozhi_mcp.h"
#include "xiaozhi_mcp_host_port.h"
#include "chime_runtime_state.h"

#include <cJSON.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

const char *const APP_VERSION = "v-test";

BatteryRuntimeStatusSnapshot battery_runtime_status_load()
{
    return {73, false, false};
}

namespace {
constexpr size_t kResponseSize = 4096;
constexpr float kExpectedTemperature = 24.5f;
constexpr float kExpectedHumidity = 56.0f;

int s_applied_volume = -1;
int s_save_count = 0;
bool s_save_result = true;
int s_alarm_disable_count = 0;
int s_alarm_call_count = 0;
XiaozhiMcpAlarmRequest s_last_alarm_request = {};
int s_pomodoro_call_count = 0;
XiaozhiMcpPomodoroRequest s_last_pomodoro_request = {};
int s_weather_city_call_count = 0;
XiaozhiMcpWeatherCityRequest s_last_weather_city_request = {};

[[noreturn]] void fail(const char *message)
{
    std::fprintf(stderr, "MCP host test failed: %s\n", message);
    std::exit(1);
}

void expect(bool condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

struct JsonOwner {
    cJSON *value = nullptr;
    JsonOwner() = default;
    explicit JsonOwner(cJSON *json) : value(json) {}
    JsonOwner(const JsonOwner &) = delete;
    JsonOwner &operator=(const JsonOwner &) = delete;
    JsonOwner(JsonOwner &&other) noexcept : value(other.value)
    {
        other.value = nullptr;
    }
    JsonOwner &operator=(JsonOwner &&other) noexcept
    {
        if (this != &other) {
            cJSON_Delete(value);
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }
    ~JsonOwner()
    {
        cJSON_Delete(value);
    }
};

JsonOwner send_request(const char *payload)
{
    char message[kResponseSize] = {};
    int written = std::snprintf(message,
                                sizeof(message),
                                "{\"session_id\":\"session-test\",\"type\":\"mcp\",\"payload\":%s}",
                                payload);
    expect(written > 0 && static_cast<size_t>(written) < sizeof(message),
           "request formatting overflow");
    // 固件 WebSocket 路径复用同一块 4 KiB PSRAM buffer 收发文本；主机测试
    // 也使用原地响应，覆盖解析树仍引用输入数据时被输出覆盖的风险。
    expect(xiaozhi_mcp_handle_message(message,
                                      static_cast<size_t>(written),
                                      "session-test",
                                      message,
                                      sizeof(message)) == kXiaozhiMcpHandledWithResponse,
           "MCP request did not produce a response");
    JsonOwner root{cJSON_Parse(message)};
    expect(root.value != nullptr, "response is not valid JSON");
    return root;
}

const cJSON *response_payload(const cJSON *root)
{
    const cJSON *type = cJSON_GetObjectItem(root, "type");
    const cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    const cJSON *payload = cJSON_GetObjectItem(root, "payload");
    expect(cJSON_IsString(type) && std::strcmp(type->valuestring, "mcp") == 0,
           "response type is not mcp");
    expect(cJSON_IsString(session_id) && std::strcmp(session_id->valuestring, "session-test") == 0,
           "response session id mismatch");
    expect(cJSON_IsObject(payload), "response payload missing");
    return payload;
}

bool tools_contains(const cJSON *tools, const char *name)
{
    const cJSON *tool = nullptr;
    cJSON_ArrayForEach(tool, tools) {
        const cJSON *tool_name = cJSON_GetObjectItem(tool, "name");
        if (cJSON_IsString(tool_name) && std::strcmp(tool_name->valuestring, name) == 0) {
            return true;
        }
    }
    return false;
}

const cJSON *tool_by_name(const cJSON *tools, const char *name)
{
    const cJSON *tool = nullptr;
    cJSON_ArrayForEach(tool, tools) {
        const cJSON *tool_name = cJSON_GetObjectItem(tool, "name");
        if (cJSON_IsString(tool_name) && std::strcmp(tool_name->valuestring, name) == 0) {
            return tool;
        }
    }
    return nullptr;
}

void expect_tool_description_contains(const cJSON *tools,
                                      const char *name,
                                      const char *expected_fragment)
{
    const cJSON *tool = tool_by_name(tools, name);
    const cJSON *description = cJSON_IsObject(tool)
                                   ? cJSON_GetObjectItem(tool, "description")
                                   : nullptr;
    expect(cJSON_IsString(description) && expected_fragment &&
               std::strstr(description->valuestring, expected_fragment) != nullptr,
           "MCP tool description missing routing rule");
}

void expect_tool_name_at(const cJSON *tools, int index, const char *expected)
{
    const cJSON *tool = cJSON_IsArray(tools) ? cJSON_GetArrayItem(tools, index) : nullptr;
    const cJSON *name = cJSON_IsObject(tool) ? cJSON_GetObjectItem(tool, "name") : nullptr;
    expect(cJSON_IsString(name) && std::strcmp(name->valuestring, expected) == 0,
           "MCP tool order changed");
}

bool string_array_contains(const cJSON *items, const char *value)
{
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, items) {
        if (cJSON_IsString(item) && std::strcmp(item->valuestring, value) == 0) {
            return true;
        }
    }
    return false;
}

JsonOwner list_tools()
{
    return send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"cursor\":\"\"},\"id\":2}");
}

const cJSON *tools_from_response(const cJSON *root)
{
    const cJSON *payload = response_payload(root);
    const cJSON *result = cJSON_GetObjectItem(payload, "result");
    const cJSON *tools = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "tools") : nullptr;
    expect(cJSON_IsArray(tools), "tools/list result missing tools array");
    return tools;
}

void test_non_mcp_and_initialize()
{
    char response[64] = "unchanged";
    constexpr char kNormalMessage[] = "{\"type\":\"stt\",\"text\":\"hello\"}";
    expect(xiaozhi_mcp_handle_message(kNormalMessage,
                                      sizeof(kNormalMessage) - 1,
                                      "session-test",
                                      response,
                                      sizeof(response)) == kXiaozhiMcpNotHandled,
           "normal STT message was consumed as MCP");

    constexpr char kSttWithMcpToken[] = "{\"type\":\"stt\",\"text\":\"mcp says \\\"mcp\\\"\"}";
    expect(xiaozhi_mcp_handle_message(kSttWithMcpToken,
                                      sizeof(kSttWithMcpToken) - 1,
                                      "session-test",
                                      response,
                                      sizeof(response)) == kXiaozhiMcpNotHandled,
           "STT text containing MCP token was consumed");
    expect(std::strcmp(response, "unchanged") == 0,
           "non-MCP message modified response buffer");

    constexpr char kMalformedMcp[] = "{\"type\":\"mcp\"";
    expect(xiaozhi_mcp_handle_message(kMalformedMcp,
                                      sizeof(kMalformedMcp) - 1,
                                      "session-test",
                                      response,
                                      sizeof(response)) == kXiaozhiMcpNotHandled,
           "malformed MCP JSON was consumed");

    constexpr char kNotification[] =
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\","
        "\"method\":\"notifications/initialized\"}}";
    expect(xiaozhi_mcp_handle_message(kNotification,
                                      sizeof(kNotification) - 1,
                                      "session-test",
                                      response,
                                      sizeof(response)) == kXiaozhiMcpHandledWithoutResponse,
           "MCP notification handling changed");
    expect(response[0] == '\0', "MCP notification left stale response text");

    constexpr char kInvalidId[] =
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\","
        "\"method\":\"initialize\",\"id\":{}}}";
    expect(xiaozhi_mcp_handle_message(kInvalidId,
                                      sizeof(kInvalidId) - 1,
                                      "session-test",
                                      response,
                                      sizeof(response)) == kXiaozhiMcpHandledWithoutResponse,
           "MCP object id handling changed");

    JsonOwner root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}");
    const cJSON *payload = response_payload(root.value);
    const cJSON *result = cJSON_GetObjectItem(payload, "result");
    const cJSON *version = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "protocolVersion") : nullptr;
    expect(cJSON_IsString(version) && std::strcmp(version->valuestring, "2024-11-05") == 0,
           "initialize protocol version mismatch");
    const cJSON *server_info = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "serverInfo") : nullptr;
    const cJSON *server_name = cJSON_IsObject(server_info) ? cJSON_GetObjectItem(server_info, "name") : nullptr;
    const cJSON *app_version = cJSON_IsObject(server_info) ? cJSON_GetObjectItem(server_info, "version") : nullptr;
    expect(cJSON_IsString(server_name) &&
               std::strcmp(server_name->valuestring, "ESP32-S3-RLCD-4.2") == 0,
           "initialize server name mismatch");
    expect(cJSON_IsString(app_version) && std::strcmp(app_version->valuestring, APP_VERSION) == 0,
           "initialize app version mismatch");
}

void test_default_tool_whitelist()
{
    JsonOwner root = list_tools();
    const cJSON *tools = tools_from_response(root.value);
    expect(cJSON_GetArraySize(tools) == 2, "default tools list must contain exactly two tools");
    expect_tool_name_at(tools, 0, "self.get_device_status");
    expect_tool_name_at(tools, 1, "self.audio_speaker.set_volume");
    expect(tools_contains(tools, "self.get_device_status"), "device status tool missing");
    expect(tools_contains(tools, "self.audio_speaker.set_volume"), "volume tool missing");
    expect(!tools_contains(tools, "self.alarm.set"), "unregistered alarm tool was exposed");
    expect(!tools_contains(tools, "self.alarm.disable"), "unregistered alarm disable tool was exposed");
    expect(!tools_contains(tools, "self.timer.set_countdown"), "unregistered countdown tool was exposed");
    expect(!tools_contains(tools, "self.pomodoro.control"), "unregistered pomodoro tool was exposed");
    expect(!tools_contains(tools, "self.weather.set_city"), "unregistered weather city tool was exposed");
    expect(!tools_contains(tools, "self.screen.set_page"), "page switch tool must not be exposed");
    expect(!tools_contains(tools, "self.chime.set_enabled"), "chime tool must not be exposed");
    expect(!tools_contains(tools, "self.offline_mode.set_enabled"), "offline tool must not be exposed");
}

void test_device_status()
{
    JsonOwner root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.get_device_status\",\"arguments\":{}},\"id\":3}");
    const cJSON *payload = response_payload(root.value);
    const cJSON *result = cJSON_GetObjectItem(payload, "result");
    const cJSON *content = cJSON_IsObject(result) ? cJSON_GetObjectItem(result, "content") : nullptr;
    const cJSON *item = cJSON_IsArray(content) ? cJSON_GetArrayItem(content, 0) : nullptr;
    const cJSON *text = cJSON_IsObject(item) ? cJSON_GetObjectItem(item, "text") : nullptr;
    expect(cJSON_IsString(text), "device status text result missing");
    JsonOwner status{cJSON_Parse(text->valuestring)};
    expect(status.value != nullptr, "device status text is not JSON");
    const cJSON *temperature = cJSON_GetObjectItem(status.value, "temperature_c");
    const cJSON *humidity = cJSON_GetObjectItem(status.value, "humidity_percent");
    const cJSON *battery = cJSON_GetObjectItem(status.value, "battery_percent");
    const cJSON *volume = cJSON_GetObjectItem(status.value, "volume_percent");
    expect(cJSON_IsNumber(temperature) && std::fabs(temperature->valuedouble - kExpectedTemperature) < 0.01,
           "temperature result mismatch");
    expect(cJSON_IsNumber(humidity) && std::fabs(humidity->valuedouble - kExpectedHumidity) < 0.01,
           "humidity result mismatch");
    expect(cJSON_IsNumber(battery) && battery->valueint == 73, "battery result mismatch");
    expect(cJSON_IsNumber(volume) && volume->valueint == 80, "volume result mismatch");
}

void test_volume_control()
{
    JsonOwner root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.audio_speaker.set_volume\",\"arguments\":{\"volume\":55}},\"id\":4}");
    const cJSON *payload = response_payload(root.value);
    expect(cJSON_IsObject(cJSON_GetObjectItem(payload, "result")), "valid volume call failed");
    expect(chime_runtime_volume_percent() == 55, "volume state was not updated");
    expect(s_applied_volume == 55, "active speaker volume was not applied");
    expect(xiaozhi_mcp_volume_save_pending(), "volume save was not marked pending");
    expect(xiaozhi_mcp_flush_pending_settings(), "pending volume save failed");
    expect(s_save_count == 1 && !xiaozhi_mcp_volume_save_pending(),
           "volume save state did not clear after persistence");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.audio_speaker.set_volume\",\"arguments\":{\"volume\":101}},\"id\":5}");
    payload = response_payload(root.value);
    expect(cJSON_IsObject(cJSON_GetObjectItem(payload, "error")), "out-of-range volume did not fail");
    expect(chime_runtime_volume_percent() == 55, "invalid volume changed device state");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"arguments\":{}},\"id\":51}");
    payload = response_payload(root.value);
    expect(cJSON_IsObject(cJSON_GetObjectItem(payload, "error")),
           "missing tool name did not fail");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":7,\"arguments\":{}},\"id\":52}");
    payload = response_payload(root.value);
    expect(cJSON_IsObject(cJSON_GetObjectItem(payload, "error")),
           "numeric tool name did not fail");
}

bool alarm_handler(const XiaozhiMcpAlarmRequest &request, char *result, size_t result_len)
{
    ++s_alarm_call_count;
    s_last_alarm_request = request;
    std::snprintf(result, result_len, "alarm %02d:%02d %s", request.hour, request.minute, request.label);
    return request.hour == 7 && request.minute == 30;
}

bool countdown_handler(const XiaozhiMcpCountdownRequest &request, char *result, size_t result_len)
{
    std::snprintf(result, result_len, "countdown %u %s", request.duration_seconds, request.label);
    return request.duration_seconds == 90;
}

bool alarm_disable_handler(char *result, size_t result_len)
{
    ++s_alarm_disable_count;
    std::snprintf(result, result_len, "alarm disabled");
    return true;
}

bool pomodoro_handler(const XiaozhiMcpPomodoroRequest &request,
                      char *result,
                      size_t result_len)
{
    ++s_pomodoro_call_count;
    s_last_pomodoro_request = request;
    std::snprintf(result,
                  result_len,
                  "pomodoro action=%d duration=%u",
                  static_cast<int>(request.action),
                  static_cast<unsigned>(request.duration_seconds));
    return true;
}

bool weather_city_handler(const XiaozhiMcpWeatherCityRequest &request,
                          char *result,
                          size_t result_len)
{
    ++s_weather_city_call_count;
    s_last_weather_city_request = request;
    std::snprintf(result, result_len, "weather city %s", request.city);
    return true;
}

void test_reserved_handlers()
{
    xiaozhi_mcp_register_alarm_handler(alarm_handler);
    xiaozhi_mcp_register_alarm_disable_handler(alarm_disable_handler);
    xiaozhi_mcp_register_countdown_handler(countdown_handler);
    xiaozhi_mcp_register_pomodoro_handler(pomodoro_handler);
    xiaozhi_mcp_register_weather_city_handler(weather_city_handler);
    JsonOwner root = list_tools();
    const cJSON *tools = tools_from_response(root.value);
    expect(cJSON_GetArraySize(tools) == 7, "registered MCP tools were not published");
    expect_tool_name_at(tools, 0, "self.get_device_status");
    expect_tool_name_at(tools, 1, "self.audio_speaker.set_volume");
    expect_tool_name_at(tools, 2, "self.alarm.set");
    expect_tool_name_at(tools, 3, "self.alarm.disable");
    expect_tool_name_at(tools, 4, "self.timer.set_countdown");
    expect_tool_name_at(tools, 5, "self.pomodoro.control");
    expect_tool_name_at(tools, 6, "self.weather.set_city");
    expect(tools_contains(tools, "self.alarm.set"), "registered alarm tool missing");
    expect(tools_contains(tools, "self.alarm.disable"), "registered alarm disable tool missing");
    expect(tools_contains(tools, "self.timer.set_countdown"), "registered countdown tool missing");
    expect(tools_contains(tools, "self.pomodoro.control"), "registered pomodoro tool missing");
    expect(tools_contains(tools, "self.weather.set_city"), "registered weather city tool missing");
    expect_tool_description_contains(tools,
                                     "self.timer.set_countdown",
                                     "self.pomodoro.control");
    expect_tool_description_contains(tools, "self.pomodoro.control", "专注");
    const cJSON *alarm_tool = tool_by_name(tools, "self.alarm.set");
    const cJSON *alarm_schema = alarm_tool ? cJSON_GetObjectItem(alarm_tool, "inputSchema") : nullptr;
    const cJSON *alarm_required = cJSON_IsObject(alarm_schema)
                                      ? cJSON_GetObjectItem(alarm_schema, "required")
                                      : nullptr;
    expect(cJSON_IsArray(alarm_required) && cJSON_GetArraySize(alarm_required) == 2,
           "alarm schema must require hour and minute");
    expect(string_array_contains(alarm_required, "hour") &&
               string_array_contains(alarm_required, "minute"),
           "alarm schema required fields mismatch");
    const cJSON *alarm_properties = cJSON_IsObject(alarm_schema)
                                        ? cJSON_GetObjectItem(alarm_schema, "properties")
                                        : nullptr;
    const cJSON *confirm_property = cJSON_IsObject(alarm_properties)
                                        ? cJSON_GetObjectItem(alarm_properties, "confirm_replace")
                                        : nullptr;
    const cJSON *confirm_type = cJSON_IsObject(confirm_property)
                                    ? cJSON_GetObjectItem(confirm_property, "type")
                                    : nullptr;
    expect(cJSON_IsString(confirm_type) && std::strcmp(confirm_type->valuestring, "boolean") == 0,
           "alarm confirmation schema must be optional boolean");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.alarm.set\",\"arguments\":{\"hour\":7,\"minute\":30,\"label\":\"wake\"}},\"id\":6}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "registered alarm handler call failed");
    expect(s_alarm_call_count == 1 && !s_last_alarm_request.confirm_replace,
           "initial alarm request unexpectedly confirmed replacement");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.alarm.set\",\"arguments\":{\"hour\":7,\"minute\":30,\"confirm_replace\":true}},\"id\":61}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "confirmed alarm handler call failed");
    expect(s_alarm_call_count == 2 && s_last_alarm_request.confirm_replace,
           "alarm replacement confirmation was not forwarded");

    int alarm_calls_before_invalid = s_alarm_call_count;
    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.alarm.set\",\"arguments\":{\"hour\":7,\"minute\":30,\"confirm_replace\":1}},\"id\":62}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "error")),
           "non-boolean alarm confirmation did not fail");
    expect(s_alarm_call_count == alarm_calls_before_invalid,
           "invalid alarm confirmation reached handler");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.alarm.disable\",\"arguments\":{}},\"id\":8}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "registered alarm disable handler call failed");
    expect(s_alarm_disable_count == 1, "alarm disable handler was not called exactly once");

    char guarded_message[kResponseSize] = {};
    int guarded_written = std::snprintf(
        guarded_message,
        sizeof(guarded_message),
        "{\"session_id\":\"session-test\",\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.alarm.disable\",\"arguments\":{}},\"id\":9}}");
    expect(guarded_written > 0 && static_cast<size_t>(guarded_written) < sizeof(guarded_message),
           "guarded alarm request formatting overflow");
    expect(xiaozhi_mcp_handle_message(guarded_message,
                                      static_cast<size_t>(guarded_written),
                                      "session-test",
                                      guarded_message,
                                      sizeof(guarded_message),
                                      false) == kXiaozhiMcpHandledWithResponse,
           "guarded alarm disable did not produce a response");
    expect(s_alarm_disable_count == 1,
           "conversation exit guard unexpectedly disabled the alarm");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.timer.set_countdown\",\"arguments\":{\"duration_seconds\":90,\"label\":\"tea\"}},\"id\":7}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "registered countdown handler call failed");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\",\"arguments\":{\"action\":\"start\"}},\"id\":10}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "default pomodoro start failed");
    expect(s_last_pomodoro_request.action == kXiaozhiMcpPomodoroStart &&
               s_last_pomodoro_request.has_duration_seconds == false &&
               s_last_pomodoro_request.duration_seconds == 1500,
           "default pomodoro duration mismatch");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\",\"arguments\":{\"action\":\"start\",\"duration_seconds\":5999}},\"id\":11}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "maximum pomodoro start failed");
    expect(s_last_pomodoro_request.has_duration_seconds &&
               s_last_pomodoro_request.duration_seconds == 5999,
           "maximum pomodoro duration mismatch");

    int calls_before_invalid = s_pomodoro_call_count;
    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\",\"arguments\":{\"action\":\"start\",\"duration_seconds\":6000}},\"id\":12}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "error")),
           "out-of-range pomodoro duration did not fail");
    expect(s_pomodoro_call_count == calls_before_invalid,
           "invalid pomodoro request reached handler");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\",\"arguments\":{\"action\":\"status\"}},\"id\":13}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "pomodoro status failed");
    expect(s_last_pomodoro_request.action == kXiaozhiMcpPomodoroStatus,
           "pomodoro status action mismatch");

    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\",\"arguments\":{\"action\":\"cancel\"}},\"id\":14}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "pomodoro cancel failed");
    expect(s_last_pomodoro_request.action == kXiaozhiMcpPomodoroCancel,
           "pomodoro cancel action mismatch");

    root = list_tools();
    tools = tools_from_response(root.value);
    const cJSON *weather_city_tool = tool_by_name(tools, "self.weather.set_city");
    const cJSON *weather_city_schema = weather_city_tool
                                           ? cJSON_GetObjectItem(weather_city_tool, "inputSchema")
                                           : nullptr;
    const cJSON *weather_city_required = cJSON_IsObject(weather_city_schema)
                                             ? cJSON_GetObjectItem(weather_city_schema, "required")
                                             : nullptr;
    expect(cJSON_IsArray(weather_city_required) &&
               string_array_contains(weather_city_required, "city"),
           "weather city schema must require city");
    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.weather.set_city\",\"arguments\":{\"city\":\"杭州市\"}},\"id\":15}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "result")),
           "weather city handler call failed");
    expect(s_weather_city_call_count == 1 &&
               std::strcmp(s_last_weather_city_request.city, "杭州市") == 0,
           "weather city request mismatch");
    const char *wrapped_weather_call =
        "{\"session_id\":\"session-test\",\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.weather.set_city\",\"arguments\":{\"city\":\"上海\"}},\"id\":17}}";
    expect(xiaozhi_mcp_message_calls_weather_city(wrapped_weather_call,
                                                   std::strlen(wrapped_weather_call)),
           "weather city MCP preflight did not recognize the wrapped tool call");
    expect(!xiaozhi_mcp_message_calls_weather_city(
               "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\"}}}",
               std::strlen("{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.pomodoro.control\"}}}")),
           "weather city MCP preflight matched another tool");
    root = send_request(
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"self.weather.set_city\",\"arguments\":{\"city\":\"\"}},\"id\":16}");
    expect(cJSON_IsObject(cJSON_GetObjectItem(response_payload(root.value), "error")),
           "empty weather city did not fail");
    expect(s_weather_city_call_count == 1, "invalid weather city reached handler");

    xiaozhi_mcp_register_alarm_handler(nullptr);
    xiaozhi_mcp_register_alarm_disable_handler(nullptr);
    xiaozhi_mcp_register_countdown_handler(nullptr);
    xiaozhi_mcp_register_pomodoro_handler(nullptr);
    xiaozhi_mcp_register_weather_city_handler(nullptr);
}
} // namespace

bool get_local_sensor_snapshot(float *temperature,
                               float *humidity,
                               int *temperature_trend,
                               int *humidity_trend)
{
    if (temperature) {
        *temperature = kExpectedTemperature;
    }
    if (humidity) {
        *humidity = kExpectedHumidity;
    }
    if (temperature_trend) {
        *temperature_trend = 0;
    }
    if (humidity_trend) {
        *humidity_trend = 0;
    }
    return true;
}

void apply_xiaozhi_speaker_volume(int volume_percent)
{
    s_applied_volume = volume_percent;
}

bool save_hourly_chime_setting()
{
    ++s_save_count;
    return s_save_result;
}

int main()
{
    test_non_mcp_and_initialize();
    test_default_tool_whitelist();
    test_device_status();
    test_volume_control();
    test_reserved_handlers();
    std::puts("Xiaozhi MCP host tests passed");
    return 0;
}
