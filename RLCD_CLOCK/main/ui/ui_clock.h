// 声明天气时钟页面构建、天气面板文本更新和对象引用清理接口。
#pragma once

void build_clock_ui();
bool set_clock_weather_panel_text(const char *city,
                                  const char *info,
                                  const char *temperature,
                                  const char *humidity,
                                  const char *icon_text);
void clear_clock_weather_panel_object_refs();
void clear_clock_local_sensor_object_refs();
void clear_clock_header_object_refs();
void clear_clock_surface_object_refs();
