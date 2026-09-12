// 提供温湿度小时历史的只读快照接口。
#pragma once

#include "sensor_history_types.h"

#include <stdint.h>

uint32_t get_hourly_sensor_history_version();
bool get_hourly_sensor_history_snapshot(HourlySensorHistoryBlob *history, uint32_t *version);
