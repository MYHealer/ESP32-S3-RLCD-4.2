// 验证双位数码字形缩放居中的普通、偏移和异常比例边界。
#include "ui_dseg_layout.h"

#include <assert.h>

int main()
{
    static_assert(dseg_scaled_position(11, 3, 4) == 8);
    static_assert(dseg_scaled_size(11, 3, 4) == 9);
    static_assert(dseg_scaled_position(-3, 3, 4) == -2);

    constexpr DsegPairLayout equal = centered_dseg_pair_layout(112,
                                                                3,
                                                                4,
                                                                0,
                                                                40,
                                                                44,
                                                                0,
                                                                40);
    static_assert(equal.first_origin_x == 24);
    static_assert(equal.second_origin_x == 57);

    constexpr DsegPairLayout offsets = centered_dseg_pair_layout(112,
                                                                  3,
                                                                  4,
                                                                  -2,
                                                                  37,
                                                                  42,
                                                                  3,
                                                                  35);
    static_assert(offsets.first_origin_x == 26);
    static_assert(offsets.second_origin_x == 57);

    constexpr DsegPairLayout invalid = centered_dseg_pair_layout(112,
                                                                  3,
                                                                  0,
                                                                  0,
                                                                  40,
                                                                  44,
                                                                  0,
                                                                  40);
    static_assert(invalid.first_origin_x == 0 && invalid.second_origin_x == 0);

    constexpr DsegGlyphBounds zero = dseg_scaled_glyph_bounds(4,
                                                               84,
                                                               3,
                                                               4,
                                                               8,
                                                               -84,
                                                               53,
                                                               84);
    static_assert(zero.x1 == 10 && zero.y1 == 21);
    static_assert(zero.x2 == 49 && zero.y2 == 83);
    static_assert(dseg_glyph_bounds_valid(zero));

    constexpr DsegGlyphBounds one = dseg_scaled_glyph_bounds(4,
                                                              84,
                                                              3,
                                                              4,
                                                              50,
                                                              -80,
                                                              11,
                                                              76);
    static_assert(one.x1 == 41 && one.y1 == 24);
    static_assert(one.x2 == 49 && one.y2 == 80);
    static_assert(!dseg_glyph_bounds_equal(zero, one));
    static_assert(dseg_glyph_bounds_overlap(zero, one));

    constexpr DsegGlyphBounds combined = dseg_union_glyph_bounds(zero, one);
    static_assert(combined.x1 == 10 && combined.y1 == 21);
    static_assert(combined.x2 == 49 && combined.y2 == 83);
    constexpr DsegGlyphBounds empty = {0, 0, -1, -1};
    static_assert(dseg_glyph_bounds_equal(dseg_union_glyph_bounds(empty, zero),
                                          zero));

    assert(equal.second_origin_x > equal.first_origin_x);
    return 0;
}
