// 声明小智 MCP 工具参数到业务请求结构的纯解析接口。
#pragma once

#include "xiaozhi_mcp.h"

#include <cJSON.h>

namespace xiaozhi_mcp_arguments {
bool parse_volume(const cJSON *arguments, int *volume);
bool parse_alarm(const cJSON *arguments, XiaozhiMcpAlarmRequest *request);
bool parse_countdown(const cJSON *arguments, XiaozhiMcpCountdownRequest *request);
bool parse_pomodoro(const cJSON *arguments, XiaozhiMcpPomodoroRequest *request);
bool parse_weather_city(const cJSON *arguments, XiaozhiMcpWeatherCityRequest *request);
} // namespace xiaozhi_mcp_arguments
