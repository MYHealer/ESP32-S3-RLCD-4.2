// 管理小智回复字幕的逐字显示、最新行窗口和下一帧调度。
#pragma once

#include "lvgl.h"

#include <stddef.h>
#include <stdint.h>

constexpr size_t kXiaozhiSubtitleTextSize = 192;
constexpr uint32_t kXiaozhiSubtitleCharacterIntervalMs = 80;

const char *xiaozhi_progressive_subtitle(bool speaking, const char *detail);
const char *xiaozhi_latest_visible_subtitle(const char *text,
                                            const lv_font_t *font,
                                            lv_coord_t max_width,
                                            lv_coord_t max_height);
uint32_t xiaozhi_subtitle_next_delay_ms();
