// MiPlay 媒体播放状态共享层实现。
//
// 使用互斥锁保护结构体字段（因为快照包含多个相关字段，
// 原子操作无法保证多字段一致性）。
#include "miplay_media_state.h"
#include "lyric_bridge.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

namespace {
SemaphoreHandle_t s_media_mutex = nullptr;
MiPlayMediaSnapshot s_media_snapshot;
uint32_t s_meta_version = 0;

SemaphoreHandle_t s_lyric_mutex = nullptr;
MiPlayLyricSnapshot s_lyric_snapshot;
uint32_t s_lyric_version = 0;

// 歌词行存储
constexpr int kMaxLyricLines = 128;
constexpr int kMaxLyricLineLen = 64;
char s_lyric_lines[kMaxLyricLines][kMaxLyricLineLen];
int s_lyric_line_count = 0;

void ensure_mutexes()
{
    if (!s_media_mutex) {
        s_media_mutex = xSemaphoreCreateMutex();
    }
    if (!s_lyric_mutex) {
        s_lyric_mutex = xSemaphoreCreateMutex();
    }
}
} // namespace

MiPlayMediaSnapshot miplay_media_snapshot_load()
{
    ensure_mutexes();
    MiPlayMediaSnapshot snap;
    if (s_media_mutex && xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = s_media_snapshot;
        xSemaphoreGive(s_media_mutex);
    }
    return snap;
}

void miplay_media_state_set(MiPlayMediaState state)
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_media_snapshot.state = state;
        s_media_snapshot.playing = (state == kMiPlayMediaPlaying);
        s_media_snapshot.paused = (state == kMiPlayMediaPaused);
        xSemaphoreGive(s_media_mutex);
    }
}

void miplay_media_meta_update(const char *title, const char *artist,
                               const char *album, int64_t duration_ms)
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (title) {
            strncpy(s_media_snapshot.title, title, sizeof(s_media_snapshot.title) - 1);
            s_media_snapshot.title[sizeof(s_media_snapshot.title) - 1] = '\0';
        }
        if (artist) {
            strncpy(s_media_snapshot.artist, artist, sizeof(s_media_snapshot.artist) - 1);
            s_media_snapshot.artist[sizeof(s_media_snapshot.artist) - 1] = '\0';
        }
        if (album) {
            strncpy(s_media_snapshot.album, album, sizeof(s_media_snapshot.album) - 1);
            s_media_snapshot.album[sizeof(s_media_snapshot.album) - 1] = '\0';
        }
        s_media_snapshot.duration_ms = duration_ms;
        s_meta_version++;
        s_media_snapshot.meta_version = s_meta_version;
        xSemaphoreGive(s_media_mutex);
    }
    // 触发歌词异步获取（锁外调用，避免死锁）
    if (title && title[0]) {
        lyric_bridge_on_meta_changed(title, artist ? artist : "");
    }
}

void miplay_media_position_update(int64_t position_ms)
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_media_snapshot.position_ms = position_ms;
        xSemaphoreGive(s_media_mutex);
    }
}

void miplay_media_clear()
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_media_snapshot = MiPlayMediaSnapshot();
        xSemaphoreGive(s_media_mutex);
    }
    lyric_bridge_clear();
}

MiPlayLyricSnapshot miplay_lyric_snapshot_load()
{
    ensure_mutexes();
    MiPlayLyricSnapshot snap;
    if (s_lyric_mutex && xSemaphoreTake(s_lyric_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = s_lyric_snapshot;
        xSemaphoreGive(s_lyric_mutex);
    }
    return snap;
}

const char *miplay_lyric_get_line(int index)
{
    ensure_mutexes();
    if (index < 0 || index >= s_lyric_line_count) {
        return nullptr;
    }
    return s_lyric_lines[index];
}

void miplay_lyric_update(int current_index)
{
    ensure_mutexes();
    if (xSemaphoreTake(s_lyric_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_lyric_snapshot.current_index = current_index;
        s_lyric_version++;
        s_lyric_snapshot.version = s_lyric_version;
        xSemaphoreGive(s_lyric_mutex);
    }
}

void miplay_lyric_set_lines(const char *const *lines, int count)
{
    ensure_mutexes();
    if (xSemaphoreTake(s_lyric_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_lyric_line_count = count < kMaxLyricLines ? count : kMaxLyricLines;
        for (int i = 0; i < s_lyric_line_count; i++) {
            if (lines[i]) {
                strncpy(s_lyric_lines[i], lines[i], kMaxLyricLineLen - 1);
                s_lyric_lines[i][kMaxLyricLineLen - 1] = '\0';
            } else {
                s_lyric_lines[i][0] = '\0';
            }
        }
        s_lyric_snapshot.current_index = -1;
        s_lyric_version++;
        s_lyric_snapshot.version = s_lyric_version;
        xSemaphoreGive(s_lyric_mutex);
    }
}

void miplay_lyric_clear()
{
    ensure_mutexes();
    if (xSemaphoreTake(s_lyric_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_lyric_line_count = 0;
        s_lyric_snapshot = MiPlayLyricSnapshot();
        xSemaphoreGive(s_lyric_mutex);
    }
}

bool miplay_media_is_active()
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        bool active = s_media_snapshot.state != kMiPlayMediaIdle;
        xSemaphoreGive(s_media_mutex);
        return active;
    }
    return false;
}

void miplay_media_toggle_pause()
{
    ensure_mutexes();
    if (xSemaphoreTake(s_media_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (s_media_snapshot.state == kMiPlayMediaPlaying) {
            s_media_snapshot.state = kMiPlayMediaPaused;
            s_media_snapshot.paused = true;
            s_media_snapshot.playing = false;
        } else if (s_media_snapshot.state == kMiPlayMediaPaused) {
            s_media_snapshot.state = kMiPlayMediaPlaying;
            s_media_snapshot.paused = false;
            s_media_snapshot.playing = true;
        }
        xSemaphoreGive(s_media_mutex);
    }
}
