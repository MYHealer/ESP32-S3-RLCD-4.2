// 声明整点提醒、全天提醒、音量和音色的一致原子运行态接口。
#pragma once

#include <stdint.h>

struct ChimeRuntimeSnapshot {
    bool hourly_enabled;
    bool all_day;
    uint8_t volume_percent;
    uint8_t sound_index;
};

ChimeRuntimeSnapshot chime_runtime_snapshot_load();
bool chime_runtime_any_enabled();
int chime_runtime_volume_percent();
int chime_runtime_sound_index();
