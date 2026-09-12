// 管理本地温湿度当前值、趋势和版本的任务级一致快照。
#pragma once

#include <stdint.h>

struct LocalSensorStateSnapshot {
    float temperature = 0.0f;
    float humidity = 0.0f;
    int temperature_trend = 0;
    int humidity_trend = 0;
    uint32_t version = 0;
    bool available = false;
};

// 读取失败时返回 false，并把非空输出重置为不可用的安全快照。
bool local_sensor_state_snapshot_load(LocalSensorStateSnapshot *snapshot);
// 互斥不可用时返回 false 并清零非空输出；状态可读但样本暂不可用时保留最近值。
bool get_local_sensor_snapshot(float *temperature,
                               float *humidity,
                               int *temperature_trend,
                               int *humidity_trend);
uint32_t local_sensor_state_version();
