// 验证小智 MCP 工具参数的类型、范围、默认值和文本边界。
#include "xiaozhi_json_owner.h"
#include "xiaozhi_mcp_arguments.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
void expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void parse(XiaozhiJsonOwner *root, const char *json)
{
    expect(root != nullptr, "test JSON owner missing");
    root->reset(cJSON_Parse(json));
    expect(static_cast<bool>(*root), "test JSON did not parse");
}

void test_volume()
{
    int volume = -1;
    XiaozhiJsonOwner root;
    parse(&root, "{\"volume\":0}");
    expect(xiaozhi_mcp_arguments::parse_volume(root.get(), &volume) && volume == 0,
           "minimum volume rejected");
    parse(&root, "{\"volume\":100}");
    expect(xiaozhi_mcp_arguments::parse_volume(root.get(), &volume) && volume == 100,
           "maximum volume rejected");
    parse(&root, "{\"volume\":50.5}");
    expect(!xiaozhi_mcp_arguments::parse_volume(root.get(), &volume),
           "fractional volume accepted");
    parse(&root, "{\"volume\":101}");
    expect(!xiaozhi_mcp_arguments::parse_volume(root.get(), &volume),
           "out-of-range volume accepted");
    expect(!xiaozhi_mcp_arguments::parse_volume(root.get(), nullptr),
           "null volume output accepted");
}

void test_alarm()
{
    XiaozhiMcpAlarmRequest request = {};
    XiaozhiJsonOwner root;
    parse(&root, "{\"hour\":0,\"minute\":59,\"label\":\"wake\"}");
    expect(xiaozhi_mcp_arguments::parse_alarm(root.get(), &request), "valid alarm rejected");
    expect(request.hour == 0 && request.minute == 59 && !request.confirm_replace &&
               std::strcmp(request.label, "wake") == 0,
           "alarm fields mismatch");
    parse(&root, "{\"hour\":23,\"minute\":0,\"confirm_replace\":true}");
    expect(xiaozhi_mcp_arguments::parse_alarm(root.get(), &request) && request.confirm_replace,
           "confirmed alarm rejected");
    parse(&root, "{\"hour\":24,\"minute\":0}");
    expect(!xiaozhi_mcp_arguments::parse_alarm(root.get(), &request),
           "invalid alarm hour accepted");
    parse(&root, "{\"hour\":7,\"minute\":30,\"confirm_replace\":1}");
    expect(!xiaozhi_mcp_arguments::parse_alarm(root.get(), &request),
           "numeric alarm confirmation accepted");

    const std::string long_label(80, 'L');
    const std::string json = "{\"hour\":7,\"minute\":30,\"label\":\"" + long_label + "\"}";
    parse(&root, json.c_str());
    expect(xiaozhi_mcp_arguments::parse_alarm(root.get(), &request),
           "long alarm label rejected");
    expect(std::strlen(request.label) == sizeof(request.label) - 1,
           "alarm label truncation changed");
}

void test_countdown()
{
    XiaozhiMcpCountdownRequest request = {};
    XiaozhiJsonOwner root;
    parse(&root, "{\"duration_seconds\":1,\"label\":\"tea\"}");
    expect(xiaozhi_mcp_arguments::parse_countdown(root.get(), &request),
           "minimum countdown rejected");
    expect(request.duration_seconds == 1 && std::strcmp(request.label, "tea") == 0,
           "countdown fields mismatch");
    parse(&root, "{\"duration_seconds\":604800}");
    expect(xiaozhi_mcp_arguments::parse_countdown(root.get(), &request) &&
               request.duration_seconds == 604800,
           "maximum countdown rejected");
    parse(&root, "{\"duration_seconds\":0}");
    expect(!xiaozhi_mcp_arguments::parse_countdown(root.get(), &request),
           "zero countdown accepted");
    parse(&root, "{\"duration_seconds\":1.5}");
    expect(!xiaozhi_mcp_arguments::parse_countdown(root.get(), &request),
           "fractional countdown accepted");
}

void test_pomodoro()
{
    XiaozhiMcpPomodoroRequest request = {};
    XiaozhiJsonOwner root;
    parse(&root, "{\"action\":\"start\"}");
    expect(xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request),
           "default pomodoro rejected");
    expect(request.action == kXiaozhiMcpPomodoroStart && !request.has_duration_seconds &&
               request.duration_seconds == 1500,
           "default pomodoro fields mismatch");
    parse(&root, "{\"action\":\"start\",\"duration_seconds\":5999}");
    expect(xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request) &&
               request.has_duration_seconds && request.duration_seconds == 5999,
           "maximum pomodoro rejected");
    parse(&root, "{\"action\":\"cancel\"}");
    expect(xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request) &&
               request.action == kXiaozhiMcpPomodoroCancel,
           "pomodoro cancel rejected");
    parse(&root, "{\"action\":\"status\"}");
    expect(xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request) &&
               request.action == kXiaozhiMcpPomodoroStatus,
           "pomodoro status rejected");
    parse(&root, "{\"action\":\"cancel\",\"duration_seconds\":1}");
    expect(!xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request),
           "cancel duration accepted");
    parse(&root, "{\"action\":\"start\",\"duration_seconds\":1.5}");
    expect(!xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request),
           "fractional pomodoro accepted");
    parse(&root, "{\"action\":\"pause\"}");
    expect(!xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &request),
           "unknown pomodoro action accepted");
}

void test_weather_city()
{
    XiaozhiMcpWeatherCityRequest request = {};
    XiaozhiJsonOwner root;
    parse(&root, "{\"city\":\"杭州市\"}");
    expect(xiaozhi_mcp_arguments::parse_weather_city(root.get(), &request) &&
               std::strcmp(request.city, "杭州市") == 0,
           "valid weather city rejected");
    parse(&root, "{\"city\":\"\"}");
    expect(!xiaozhi_mcp_arguments::parse_weather_city(root.get(), &request),
           "empty weather city accepted");
    const std::string maximum_city(63, 'C');
    const std::string maximum_json = "{\"city\":\"" + maximum_city + "\"}";
    parse(&root, maximum_json.c_str());
    expect(xiaozhi_mcp_arguments::parse_weather_city(root.get(), &request) &&
               std::strlen(request.city) == 63,
           "maximum weather city length rejected");
    const std::string oversized_city(64, 'C');
    const std::string oversized_json = "{\"city\":\"" + oversized_city + "\"}";
    parse(&root, oversized_json.c_str());
    expect(!xiaozhi_mcp_arguments::parse_weather_city(root.get(), &request),
           "oversized weather city accepted");
}

void test_malformed_argument_roots()
{
    int volume = -1;
    XiaozhiMcpAlarmRequest alarm = {};
    XiaozhiMcpCountdownRequest countdown = {};
    XiaozhiMcpPomodoroRequest pomodoro = {};
    XiaozhiMcpWeatherCityRequest city = {};
    expect(!xiaozhi_mcp_arguments::parse_volume(nullptr, &volume),
           "null volume arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_alarm(nullptr, &alarm),
           "null alarm arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_countdown(nullptr, &countdown),
           "null countdown arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_pomodoro(nullptr, &pomodoro),
           "null pomodoro arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_weather_city(nullptr, &city),
           "null weather city arguments accepted");

    XiaozhiJsonOwner root;
    parse(&root, "[]");
    expect(!xiaozhi_mcp_arguments::parse_volume(root.get(), &volume),
           "array volume arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_pomodoro(root.get(), &pomodoro),
           "array pomodoro arguments accepted");
    expect(!xiaozhi_mcp_arguments::parse_weather_city(root.get(), &city),
           "array weather city arguments accepted");
}
} // namespace

int main()
{
    test_volume();
    test_alarm();
    test_countdown();
    test_pomodoro();
    test_weather_city();
    test_malformed_argument_roots();
    std::puts("Xiaozhi MCP argument host tests passed");
    return 0;
}
