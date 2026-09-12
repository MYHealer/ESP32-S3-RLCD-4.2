// 提供 OTA 任务与 UI/网络消费者之间的一致运行态快照。
#pragma once

#include "ota_flow_policy.h"

#include <freertos/FreeRTOS.h>

inline constexpr int kOtaStatusLen = 96;

struct OtaRuntimeSnapshot {
    int state = kOtaIdle;
    int progress = -1;
    int speed_kbps = -1;
    TickType_t status_until_tick = 0;
    bool status_hold_set = false;
    bool reboot_pending = false;
    char status[kOtaStatusLen] = {};
};

struct OtaRuntimeTimingSnapshot {
    int state = kOtaIdle;
    TickType_t status_until_tick = 0;
    bool status_hold_set = false;
};

struct OtaRuntimeFlagsSnapshot {
    int state = kOtaIdle;
    bool reboot_pending = false;
};

void ota_runtime_snapshot_load(OtaRuntimeSnapshot *snapshot);
void ota_runtime_timing_snapshot_load(OtaRuntimeTimingSnapshot *snapshot);
OtaRuntimeFlagsSnapshot ota_runtime_flags_load();
int ota_runtime_state_load();
