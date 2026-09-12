// 声明声音设置完整快照的内部发布接口，仅供配置事务和启动加载使用。
#pragma once

#include "chime_runtime_state.h"

void chime_runtime_snapshot_store(const ChimeRuntimeSnapshot &snapshot);
void chime_runtime_volume_percent_store(int volume_percent);
