// MiPlay 媒体播放状态共享层。
//
// 提供原子快照，供 UI 任务读取、MiPlay 回调写入。
// 所有字段通过原子操作或临界区保护，跨 Core 0（airkan/网络）和
// Core 1（UI 任务）安全访问。
#pragma once

#include <cstdint>
#include <atomic>

enum MiPlayMediaState : uint8_t {
    kMiPlayMediaIdle = 0,
    kMiPlayMediaPlaying = 1,
    kMiPlayMediaPaused = 2,
};

struct MiPlayMediaSnapshot {
    MiPlayMediaState state = kMiPlayMediaIdle;
    bool playing = false;
    bool paused = false;
    int64_t position_ms = 0;
    int64_t duration_ms = 0;
    char title[64] = "";
    char artist[64] = "";
    char album[64] = "";
    uint32_t meta_version = 0;
};

struct MiPlayLyricSnapshot {
    int current_index = -1;
    uint32_t version = 0;
};

// 媒体状态读取
MiPlayMediaSnapshot miplay_media_snapshot_load();

// 媒体状态写入（由 MiPlay 回调调用）
void miplay_media_state_set(MiPlayMediaState state);
void miplay_media_meta_update(const char *title, const char *artist,
                               const char *album, int64_t duration_ms);
void miplay_media_position_update(int64_t position_ms);
void miplay_media_clear();

// 歌词
MiPlayLyricSnapshot miplay_lyric_snapshot_load();
const char *miplay_lyric_get_line(int index);
void miplay_lyric_update(int current_index);
void miplay_lyric_set_lines(const char *const *lines, int count);
void miplay_lyric_clear();

// 播放/暂停切换（遥控器调用）
bool miplay_media_is_active();
void miplay_media_toggle_pause();
