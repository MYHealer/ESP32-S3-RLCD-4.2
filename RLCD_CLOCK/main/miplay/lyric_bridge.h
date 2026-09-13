// 歌词获取桥接层 — 连接 lyrics_fetch 与 miplay_media_state。
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 元数据变化时调用，触发异步歌词获取
 */
void lyric_bridge_on_meta_changed(const char *title, const char *artist);

/**
 * @brief 周期性调用（建议 ~500ms），同步歌词状态到 miplay
 */
void lyric_bridge_poll(void);

/**
 * @brief 清除歌词状态
 */
void lyric_bridge_clear(void);

#ifdef __cplusplus
}
#endif
