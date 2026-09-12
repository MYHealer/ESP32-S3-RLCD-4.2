// 计算双位数码字形缩放后的水平居中布局。
#pragma once

struct DsegPairLayout {
    int first_origin_x;
    int second_origin_x;
};

struct DsegGlyphBounds {
    int x1;
    int y1;
    int x2;
    int y2;
};

constexpr int dseg_scaled_position(int value, int scale_num, int scale_den)
{
    return scale_den > 0 ? (value * scale_num) / scale_den : 0;
}

constexpr int dseg_scaled_size(int value, int scale_num, int scale_den)
{
    return scale_den > 0 ? (value * scale_num + scale_den - 1) / scale_den : 0;
}

constexpr DsegGlyphBounds dseg_scaled_glyph_bounds(int origin_x,
                                                    int origin_y,
                                                    int scale_num,
                                                    int scale_den,
                                                    int x_offset,
                                                    int y_offset,
                                                    int width,
                                                    int height)
{
    const int scaled_width = dseg_scaled_size(width, scale_num, scale_den);
    const int scaled_height = dseg_scaled_size(height, scale_num, scale_den);
    const int x1 = origin_x + dseg_scaled_position(x_offset, scale_num, scale_den);
    const int y1 = origin_y + dseg_scaled_position(y_offset, scale_num, scale_den);
    return scale_num > 0 && scale_den > 0 && scaled_width > 0 && scaled_height > 0
               ? DsegGlyphBounds{x1,
                                 y1,
                                 x1 + scaled_width - 1,
                                 y1 + scaled_height - 1}
               : DsegGlyphBounds{0, 0, -1, -1};
}

constexpr bool dseg_glyph_bounds_valid(const DsegGlyphBounds &bounds)
{
    return bounds.x1 <= bounds.x2 && bounds.y1 <= bounds.y2;
}

constexpr bool dseg_glyph_bounds_equal(const DsegGlyphBounds &left,
                                       const DsegGlyphBounds &right)
{
    return left.x1 == right.x1 && left.y1 == right.y1 &&
           left.x2 == right.x2 && left.y2 == right.y2;
}

constexpr bool dseg_glyph_bounds_overlap(const DsegGlyphBounds &left,
                                         const DsegGlyphBounds &right)
{
    return dseg_glyph_bounds_valid(left) &&
           dseg_glyph_bounds_valid(right) &&
           left.x1 <= right.x2 && left.x2 >= right.x1 &&
           left.y1 <= right.y2 && left.y2 >= right.y1;
}

constexpr DsegGlyphBounds dseg_union_glyph_bounds(const DsegGlyphBounds &left,
                                                  const DsegGlyphBounds &right)
{
    if (!dseg_glyph_bounds_valid(left)) {
        return right;
    }
    if (!dseg_glyph_bounds_valid(right)) {
        return left;
    }
    return {
        left.x1 < right.x1 ? left.x1 : right.x1,
        left.y1 < right.y1 ? left.y1 : right.y1,
        left.x2 > right.x2 ? left.x2 : right.x2,
        left.y2 > right.y2 ? left.y2 : right.y2,
    };
}

constexpr DsegPairLayout centered_dseg_pair_layout(int container_width,
                                                    int scale_num,
                                                    int scale_den,
                                                    int first_x_offset,
                                                    int first_width,
                                                    int first_x_advance,
                                                    int second_x_offset,
                                                    int second_width)
{
    if (scale_num <= 0 || scale_den <= 0) {
        return {0, 0};
    }
    const int second_unshifted_origin = dseg_scaled_position(first_x_advance,
                                                             scale_num,
                                                             scale_den);
    const int first_left = dseg_scaled_position(first_x_offset, scale_num, scale_den);
    const int second_left = second_unshifted_origin +
                            dseg_scaled_position(second_x_offset, scale_num, scale_den);
    const int left = first_left < second_left ? first_left : second_left;
    const int first_right = first_left + dseg_scaled_size(first_width, scale_num, scale_den);
    const int second_right = second_left + dseg_scaled_size(second_width, scale_num, scale_den);
    const int right = first_right > second_right ? first_right : second_right;
    const int first_origin = (container_width - (right - left)) / 2 - left;
    return {first_origin, first_origin + second_unshifted_origin};
}
