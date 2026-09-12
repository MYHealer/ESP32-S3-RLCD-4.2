// 验证生产小智字幕模块的 UTF-8 逐字显示、窗口裁剪和调度边界。
#include "ui_xiaozhi_subtitle.h"

#include <assert.h>
#include <string.h>

namespace {
uint32_t s_tick = 0;
int s_measure_calls = 0;

void set_tick(uint32_t tick)
{
    s_tick = tick;
}
} // namespace

uint32_t lv_tick_get()
{
    return s_tick;
}

uint32_t lv_tick_elaps(uint32_t previous)
{
    return s_tick - previous;
}

void lv_txt_get_size(lv_point_t *size,
                     const char *text,
                     const lv_font_t *,
                     lv_coord_t,
                     lv_coord_t,
                     lv_coord_t,
                     lv_text_flag_t)
{
    ++s_measure_calls;
    const size_t bytes = strlen(text);
    size->x = static_cast<lv_coord_t>(bytes < 4 ? bytes : 4);
    size->y = static_cast<lv_coord_t>(((bytes + 3) / 4) * 10);
}

uint32_t _lv_txt_get_next_line(const char *text,
                               const lv_font_t *,
                               lv_coord_t,
                               lv_coord_t,
                               lv_coord_t *,
                               lv_text_flag_t)
{
    const size_t bytes = strlen(text);
    return static_cast<uint32_t>(bytes < 4 ? bytes : 4);
}

int main()
{
    set_tick(100);
    assert(strcmp(xiaozhi_progressive_subtitle(true, "A中B"), "A") == 0);
    assert(xiaozhi_subtitle_next_delay_ms() == 80);

    set_tick(179);
    assert(strcmp(xiaozhi_progressive_subtitle(true, "A中B"), "A") == 0);
    assert(xiaozhi_subtitle_next_delay_ms() == 1);

    set_tick(180);
    assert(strcmp(xiaozhi_progressive_subtitle(true, "A中B"), "A中") == 0);
    assert(xiaozhi_subtitle_next_delay_ms() == 80);

    set_tick(260);
    assert(strcmp(xiaozhi_progressive_subtitle(true, "A中B"), "A中B") == 0);
    assert(xiaozhi_subtitle_next_delay_ms() == 0);

    assert(strcmp(xiaozhi_progressive_subtitle(false, "完整回复"), "完整回复") == 0);
    assert(xiaozhi_subtitle_next_delay_ms() == 0);

    set_tick(500);
    assert(strcmp(xiaozhi_progressive_subtitle(true, "新内容"), "新") == 0);

    char boundary_text[kXiaozhiSubtitleTextSize + 2] = {};
    memset(boundary_text, 'A', kXiaozhiSubtitleTextSize - 2);
    memcpy(boundary_text + kXiaozhiSubtitleTextSize - 2, "中", 3);
    boundary_text[kXiaozhiSubtitleTextSize + 1] = '\0';
    set_tick(1000);
    assert(strcmp(xiaozhi_progressive_subtitle(true, boundary_text), "A") == 0);
    set_tick(20000);
    const char *boundary_visible =
        xiaozhi_progressive_subtitle(true, boundary_text);
    assert(strlen(boundary_visible) == kXiaozhiSubtitleTextSize - 2);
    for (size_t index = 0; boundary_visible[index] != '\0'; ++index) {
        assert(boundary_visible[index] == 'A');
    }
    assert(xiaozhi_subtitle_next_delay_ms() == 0);

    lv_font_t font = {};
    s_measure_calls = 0;
    assert(strcmp(xiaozhi_latest_visible_subtitle("abcdefghij", &font, 4, 20),
                  "efghij") == 0);
    const int measured = s_measure_calls;
    assert(measured == 2);
    assert(strcmp(xiaozhi_latest_visible_subtitle("abcdefghij", &font, 4, 20),
                  "efghij") == 0);
    assert(s_measure_calls == measured);

    assert(strcmp(xiaozhi_latest_visible_subtitle("abcd", &font, 4, 20),
                  "abcd") == 0);
    assert(strcmp(xiaozhi_latest_visible_subtitle(nullptr, &font, 4, 20), "") == 0);
    return 0;
}
