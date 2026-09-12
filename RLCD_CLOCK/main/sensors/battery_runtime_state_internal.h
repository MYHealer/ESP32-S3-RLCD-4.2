// 声明仅供初始化与电池采样发布者使用的运行态写接口。
#pragma once

#include "battery_runtime_state.h"

bool battery_runtime_state_init();
void battery_runtime_snapshot_store(const BatteryRuntimeSnapshot &snapshot);
bool battery_low_mode_for_percent(bool current_mode,
                                  int percent,
                                  int enter_percent,
                                  int exit_percent);
