// 供应用初始化和传感器采样业务内部发布本地温湿度完整快照。
#pragma once

#include "local_sensor_state.h"

bool init_local_sensor_state();
bool local_sensor_state_publish_sample(float temperature,
                                       float humidity,
                                       int temperature_trend,
                                       int humidity_trend,
                                       bool *state_changed = nullptr);
bool local_sensor_state_publish_unavailable(bool *state_changed = nullptr);
