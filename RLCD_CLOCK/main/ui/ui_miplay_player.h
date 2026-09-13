// 声明 MiPlay 音乐播放页的构建、更新和清理接口。
#pragma once

#include <time.h>

void build_miplay_player_page();
bool update_miplay_player_page(const struct tm &local);
void clear_miplay_player_object_refs();

// MiPlay 播放控制回调（函数指针，由 miplay 模块注册）
typedef void (*MiPlayControlFn)(void);
void miplay_register_control_callbacks(MiPlayControlFn prev,
                                        MiPlayControlFn play_pause,
                                        MiPlayControlFn next);
void miplay_trigger_prev(void);
void miplay_trigger_play_pause(void);
void miplay_trigger_next(void);
