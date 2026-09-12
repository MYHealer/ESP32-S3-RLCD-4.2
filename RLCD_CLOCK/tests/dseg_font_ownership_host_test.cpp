// 验证断码字体在多个编译单元之间共享同一份只读对象。
#include "dseg_digits.h"

#include <assert.h>

const void *dseg84_bitmap_owner_a();
const void *dseg84_glyph_owner_a();
const void *dseg84_font_owner_a();
const void *dseg36_bitmap_owner_a();
const void *dseg36_glyph_owner_a();
const void *dseg36_font_owner_a();

const void *dseg84_bitmap_owner_b();
const void *dseg84_glyph_owner_b();
const void *dseg84_font_owner_b();
const void *dseg36_bitmap_owner_b();
const void *dseg36_glyph_owner_b();
const void *dseg36_font_owner_b();

int main()
{
    assert(dseg84_bitmap_owner_a() == dseg84_bitmap_owner_b());
    assert(dseg84_glyph_owner_a() == dseg84_glyph_owner_b());
    assert(dseg84_font_owner_a() == dseg84_font_owner_b());
    assert(dseg36_bitmap_owner_a() == dseg36_bitmap_owner_b());
    assert(dseg36_glyph_owner_a() == dseg36_glyph_owner_b());
    assert(dseg36_font_owner_a() == dseg36_font_owner_b());

    assert(kDSEG84Font.bitmap == dseg84_bitmap_owner_a());
    assert(kDSEG84Font.glyphs == dseg84_glyph_owner_a());
    assert(kDSEG36Font.bitmap == dseg36_bitmap_owner_a());
    assert(kDSEG36Font.glyphs == dseg36_glyph_owner_a());
    return 0;
}
