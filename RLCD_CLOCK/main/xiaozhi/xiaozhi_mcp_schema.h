// 声明小智 MCP 工具名称以及 initialize/tools-list JSON 构建入口。
#pragma once

#include "cJSON.h"

namespace xiaozhi_mcp_schema {

extern const char kDeviceStatusTool[];
extern const char kSetVolumeTool[];
extern const char kSetAlarmTool[];
extern const char kDisableAlarmTool[];
extern const char kSetCountdownTool[];
extern const char kPomodoroControlTool[];
extern const char kSetWeatherCityTool[];

cJSON *create_tools_list_result(bool alarm_enabled,
                                bool alarm_disable_enabled,
                                bool countdown_enabled,
                                bool pomodoro_enabled,
                                bool weather_city_enabled);
cJSON *create_initialize_result(const char *app_version);

} // namespace xiaozhi_mcp_schema
