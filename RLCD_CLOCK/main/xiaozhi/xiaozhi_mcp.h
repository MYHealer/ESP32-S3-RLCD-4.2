// 声明小智设备端 MCP 工具、消息分发和未来提醒功能注册接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

enum XiaozhiMcpMessageResult {
    kXiaozhiMcpNotHandled = 0,
    kXiaozhiMcpHandledWithoutResponse,
    kXiaozhiMcpHandledWithResponse,
};

struct XiaozhiMcpAlarmRequest {
    int hour;
    int minute;
    bool confirm_replace;
    char label[64];
};

struct XiaozhiMcpCountdownRequest {
    uint32_t duration_seconds;
    char label[64];
};

struct XiaozhiMcpWeatherCityRequest {
    char city[64];
};

enum XiaozhiMcpPomodoroAction {
    kXiaozhiMcpPomodoroStart = 0,
    kXiaozhiMcpPomodoroCancel,
    kXiaozhiMcpPomodoroStatus,
};

struct XiaozhiMcpPomodoroRequest {
    XiaozhiMcpPomodoroAction action;
    bool has_duration_seconds;
    uint32_t duration_seconds;
};

using XiaozhiMcpAlarmHandler = bool (*)(const XiaozhiMcpAlarmRequest &request,
                                        char *result,
                                        size_t result_len);
using XiaozhiMcpAlarmDisableHandler = bool (*)(char *result, size_t result_len);
using XiaozhiMcpCountdownHandler = bool (*)(const XiaozhiMcpCountdownRequest &request,
                                            char *result,
                                            size_t result_len);
using XiaozhiMcpWeatherCityHandler = bool (*)(const XiaozhiMcpWeatherCityRequest &request,
                                              char *result,
                                              size_t result_len);
using XiaozhiMcpPomodoroHandler = bool (*)(const XiaozhiMcpPomodoroRequest &request,
                                           char *result,
                                           size_t result_len);

XiaozhiMcpMessageResult xiaozhi_mcp_handle_message(const char *message,
                                                   size_t message_len,
                                                   const char *session_id,
                                                   char *response,
                                                   size_t response_len,
                                                   bool allow_alarm_disable = true);
bool xiaozhi_mcp_message_calls_weather_city(const char *message, size_t message_len);
bool xiaozhi_mcp_volume_save_pending();
bool xiaozhi_mcp_flush_pending_settings();

// 闹钟和倒计时业务尚未实现时不注册 handler，也不会向服务端公布对应工具。
void xiaozhi_mcp_register_alarm_handler(XiaozhiMcpAlarmHandler handler);
void xiaozhi_mcp_register_alarm_disable_handler(XiaozhiMcpAlarmDisableHandler handler);
void xiaozhi_mcp_register_countdown_handler(XiaozhiMcpCountdownHandler handler);
void xiaozhi_mcp_register_pomodoro_handler(XiaozhiMcpPomodoroHandler handler);
void xiaozhi_mcp_register_weather_city_handler(XiaozhiMcpWeatherCityHandler handler);
