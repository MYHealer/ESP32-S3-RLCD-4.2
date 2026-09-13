// 共享 CJK fallback 字体链：zh_font_16 → 补丁(假名/韩文/拉丁扩展) → Montserrat
// 用 RAM 副本修改 fallback 指针（flash const 不可写）。
#include "ui_fonts.h"

static lv_font_t s_font_with_supplement;
static lv_font_t s_supplement_ram;
static bool s_ready = false;

const lv_font_t *get_cjk_font_with_supplement(void)
{
    if (!s_ready) {
        s_font_with_supplement = zh_font_16;
        s_supplement_ram = lv_font_simsun_16_supplement;
        s_supplement_ram.fallback = zh_font_16.fallback; /* montserrat_14 */
        s_font_with_supplement.fallback = &s_supplement_ram;
        s_ready = true;
    }
    return &s_font_with_supplement;
}
