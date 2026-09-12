// 验证 SDL 分段进度条的边界钳制和全天 60 段换算。
#include <assert.h>

#include "sdl_preview_progress_math.h"

int main()
{
    using namespace sdl_preview_progress;

    static_assert(clamp_filled_segments(-1) == 0);
    static_assert(clamp_filled_segments(0) == 0);
    static_assert(clamp_filled_segments(59) == 59);
    static_assert(clamp_filled_segments(60) == 60);
    static_assert(clamp_filled_segments(61) == 60);

    assert(filled_segments_for_day(0, 0, 0) == 0);
    assert(filled_segments_for_day(6, 0, 0) == 15);
    assert(filled_segments_for_day(12, 0, 0) == 30);
    assert(filled_segments_for_day(18, 0, 0) == 45);
    assert(filled_segments_for_day(23, 59, 59) == 59);
    return 0;
}
