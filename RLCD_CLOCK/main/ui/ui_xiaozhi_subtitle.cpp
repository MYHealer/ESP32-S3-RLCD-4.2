// 实现小智回复字幕的 UTF-8 逐字显示与固定区域最新行裁剪。
#include "ui_xiaozhi_subtitle.h"

#include "xiaozhi_text_utils.h"

#include <esp_attr.h>
#include <string.h>

namespace {
EXT_RAM_BSS_ATTR char s_target[kXiaozhiSubtitleTextSize] = {};
EXT_RAM_BSS_ATTR char s_visible[kXiaozhiSubtitleTextSize] = {};
EXT_RAM_BSS_ATTR char s_window[kXiaozhiSubtitleTextSize] = {};
EXT_RAM_BSS_ATTR char s_window_source[kXiaozhiSubtitleTextSize] = {};
size_t s_visible_bytes = 0;
size_t s_visible_characters = 0;
uint32_t s_last_tick = 0;
const lv_font_t *s_window_font = nullptr;
lv_coord_t s_window_width = 0;
lv_coord_t s_window_height = 0;

size_t utf8_codepoint_size(const char *text)
{
    if (!text || text[0] == '\0') {
        return 0;
    }
    const unsigned char first = static_cast<unsigned char>(text[0]);
    if (first < 0x80) return 1;
    if ((first & 0xe0) == 0xc0) return 2;
    if ((first & 0xf0) == 0xe0) return 3;
    if ((first & 0xf8) == 0xf0) return 4;
    return 1;
}

void reset_progressive_subtitle()
{
    s_target[0] = '\0';
    s_visible[0] = '\0';
    s_visible_bytes = 0;
    s_visible_characters = 0;
}

void reveal_next_character()
{
    const size_t target_len = strlen(s_target);
    if (s_visible_bytes >= target_len) {
        return;
    }
    const size_t character_size = utf8_codepoint_size(s_target + s_visible_bytes);
    if (character_size == 0 ||
        s_visible_bytes + character_size > target_len ||
        s_visible_bytes + character_size >= sizeof(s_visible)) {
        return;
    }
    memcpy(s_visible + s_visible_bytes,
           s_target + s_visible_bytes,
           character_size);
    s_visible_bytes += character_size;
    ++s_visible_characters;
    s_visible[s_visible_bytes] = '\0';
}
} // namespace

const char *xiaozhi_progressive_subtitle(bool speaking, const char *detail)
{
    detail = detail ? detail : "";
    if (!speaking) {
        reset_progressive_subtitle();
        return detail;
    }

    const uint32_t now = lv_tick_get();
    bool target_changed = false;
    if (strcmp(s_target, detail) != 0) {
        char normalized_detail[kXiaozhiSubtitleTextSize] = {};
        xiaozhi_protocol::utf8_safe_copy(
            normalized_detail, sizeof(normalized_detail), detail);
        if (strcmp(s_target, normalized_detail) != 0) {
            memcpy(s_target, normalized_detail, sizeof(s_target));
            target_changed = true;
        }
    }
    if (target_changed) {
        s_visible[0] = '\0';
        s_visible_bytes = 0;
        s_visible_characters = 0;
        s_last_tick = now;
        reveal_next_character();
    } else {
        const uint32_t elapsed = lv_tick_elaps(s_last_tick);
        const size_t due_characters = 1 + elapsed / kXiaozhiSubtitleCharacterIntervalMs;
        const size_t target_len = strlen(s_target);
        while (s_visible_characters < due_characters &&
               s_visible_bytes < target_len) {
            reveal_next_character();
        }
    }
    return s_visible;
}

const char *xiaozhi_latest_visible_subtitle(const char *text,
                                            const lv_font_t *font,
                                            lv_coord_t max_width,
                                            lv_coord_t max_height)
{
    text = text ? text : "";
    if (strcmp(s_window_source, text) == 0 &&
        s_window_font == font &&
        s_window_width == max_width &&
        s_window_height == max_height) {
        return s_window;
    }

    const char *window_start = text;
    lv_point_t text_size = {};
    lv_txt_get_size(&text_size,
                    window_start,
                    font,
                    0,
                    0,
                    max_width,
                    LV_TEXT_FLAG_NONE);
    // Drop complete wrapped lines until the newest suffix fits the panel.
    while (text_size.y > max_height && window_start[0] != '\0') {
        const uint32_t first_line_bytes = _lv_txt_get_next_line(window_start,
                                                               font,
                                                               0,
                                                               max_width,
                                                               nullptr,
                                                               LV_TEXT_FLAG_NONE);
        if (first_line_bytes == 0 || window_start[first_line_bytes] == '\0') {
            break;
        }
        window_start += first_line_bytes;
        lv_txt_get_size(&text_size,
                        window_start,
                        font,
                        0,
                        0,
                        max_width,
                        LV_TEXT_FLAG_NONE);
    }
    strlcpy(s_window_source, text, sizeof(s_window_source));
    strlcpy(s_window, window_start, sizeof(s_window));
    s_window_font = font;
    s_window_width = max_width;
    s_window_height = max_height;
    return s_window;
}

uint32_t xiaozhi_subtitle_next_delay_ms()
{
    const size_t target_len = strlen(s_target);
    if (target_len == 0 || s_visible_bytes >= target_len) {
        return 0;
    }
    const uint32_t elapsed = lv_tick_elaps(s_last_tick);
    const uint32_t next_character_due =
        static_cast<uint32_t>(s_visible_characters) * kXiaozhiSubtitleCharacterIntervalMs;
    return elapsed >= next_character_due ? 1 : next_character_due - elapsed;
}
