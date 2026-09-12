// 返回第二个编译单元看到的断码字体对象地址。
#include "dseg_digits.h"

const void *dseg84_bitmap_owner_b()
{
    return kDSEG84Bitmaps;
}

const void *dseg84_glyph_owner_b()
{
    return kDSEG84Glyphs;
}

const void *dseg84_font_owner_b()
{
    return &kDSEG84Font;
}

const void *dseg36_bitmap_owner_b()
{
    return kDSEG36Bitmaps;
}

const void *dseg36_glyph_owner_b()
{
    return kDSEG36Glyphs;
}

const void *dseg36_font_owner_b()
{
    return &kDSEG36Font;
}
