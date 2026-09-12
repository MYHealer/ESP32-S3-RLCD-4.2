// 声明跨任务一致读取的电池运行态快照。
#pragma once

#include <stdint.h>
#include <time.h>

struct BatteryRuntimeSnapshot {
    int percent = -1;
    float voltage = -1.0f;
    bool charging = false;
    bool animation_complete = false;
    time_t last_full_charge_time = 0;
    uint32_t version = 0;
    bool low_battery_mode = false;
};

struct BatteryRuntimeStatusSnapshot {
    int percent = -1;
    bool charging = false;
    bool low_battery_mode = false;
};

// 读取失败时返回 false，并保留调用方已有内容。
bool battery_runtime_snapshot_load(BatteryRuntimeSnapshot *out);
BatteryRuntimeStatusSnapshot battery_runtime_status_load();
uint32_t battery_runtime_version_load();
int battery_percent_load();
bool battery_low_mode_load();
