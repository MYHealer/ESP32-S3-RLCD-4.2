// 歌词获取桥接层：连接 lyrics_fetch (C) 与 miplay_media_state (C++)。
//
// 工作流：
//   1. miplay_media_meta_update 触发 → 调 lyrics_fetch_async 发起后台获取
//   2. 歌词获取完成后，lyrics_get_data()->loaded 变 true
//   3. 周期性轮询：将 lyric_data_t.lines[] 推入 miplay_lyric_set_lines
//   4. 播放进度更新时：lyrics_get_current_line → miplay_lyric_update
//
// 本文件编译为 C++，可直接调用两侧 API。
#include "miplay_media_state.h"

extern "C" {
#include "lyrics_fetch.h"
}

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>

static constexpr char kTag[] = "lyric_bridge";

// 上次推送的 meta_version，用于检测切歌
static uint32_t s_last_meta_ver = 0;
// 上次推送的 lyric_data count，用于检测歌词加载完成
static int s_last_lyric_count = -1;
// 上次同步的 position 和 lyric index
static int64_t s_last_sync_pos_ms = -1;
static int s_last_sync_index = -1;

// 初始化标记
static bool s_bridge_inited = false;

// ── 将 lyric_data_t 推入 miplay_lyric_set_lines ──
static void push_lyrics_to_miplay(void)
{
    const lyric_data_t *data = lyrics_get_data();
    if (!data || !data->loaded || data->count <= 0) {
        return;
    }

    // 已推送过相同数量的行，跳过
    if (s_last_lyric_count == data->count) {
        return;
    }
    s_last_lyric_count = data->count;

    // 构造指针数组
    const char *lines[LYRIC_MAX_LINES];
    for (int i = 0; i < data->count; i++) {
        lines[i] = data->lines[i].text;
    }
    miplay_lyric_set_lines(lines, data->count);
    ESP_LOGI(kTag, "Pushed %d lyric lines to miplay", data->count);
}

// ── 公共 API：元数据变化时调用（触发歌词获取）──
extern "C" void lyric_bridge_on_meta_changed(const char *title, const char *artist)
{
    if (!s_bridge_inited) {
        lyrics_init();
        s_bridge_inited = true;
    }

    if (!title || !title[0]) return;

    ESP_LOGI(kTag, "Meta changed: '%s' - '%s', triggering lyrics fetch", title, artist ? artist : "");
    /* 先清除旧歌词，避免切歌间隙 lyrics_get_current_line 仍返回上一首末行 */
    lyrics_clear();
    miplay_lyric_clear();
    s_last_lyric_count = -1;
    s_last_sync_pos_ms = -1;
    s_last_sync_index = -1;
    lyrics_fetch_async(title, artist ? artist : "", 0);
}

// ── 公共 API：周期性调用（UI 刷新循环中，~500ms 一次）──
extern "C" void lyric_bridge_poll(void)
{
    // 1. 检查歌词是否新加载完成
    push_lyrics_to_miplay();

    // 2. 同步播放进度 → 歌词行索引
    MiPlayMediaSnapshot snap = miplay_media_snapshot_load();
    if (snap.state == kMiPlayMediaIdle) return;

    int64_t pos_ms = snap.position_ms;
    // 位置变化小于 200ms 时跳过，减少无意义更新
    if (pos_ms == s_last_sync_pos_ms) return;
    s_last_sync_pos_ms = pos_ms;

    int line_idx = lyrics_get_current_line((int)pos_ms);
    if (line_idx != s_last_sync_index) {
        s_last_sync_index = line_idx;
        miplay_lyric_update(line_idx);
    }
}

// ── 公共 API：清除时调用 ──
extern "C" void lyric_bridge_clear(void)
{
    lyrics_clear();
    miplay_lyric_clear();
    s_last_meta_ver = 0;
    s_last_lyric_count = -1;
    s_last_sync_pos_ms = -1;
    s_last_sync_index = -1;
    ESP_LOGI(kTag, "Lyric bridge cleared");
}
