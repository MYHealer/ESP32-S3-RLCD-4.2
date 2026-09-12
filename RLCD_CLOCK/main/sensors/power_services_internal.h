// 声明仅供音频所有者控制的高性能电源锁写入口。
#pragma once

#include "power_services.h"

void init_power_management();
bool set_audio_performance_mode(bool enabled);
