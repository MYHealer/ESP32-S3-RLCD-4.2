// 提供 SDL 预览进度条共用的纯数值边界和全天进度计算。
#pragma once

namespace sdl_preview_progress {

inline constexpr int kSegmentCount = 60;

constexpr int clamp_filled_segments(int filled)
{
    return filled < 0 ? 0 : (filled > kSegmentCount ? kSegmentCount : filled);
}

constexpr int filled_segments_for_day(int hour, int minute, int second)
{
    const int seconds_of_day = hour * 3600 + minute * 60 + second;
    return (seconds_of_day * kSegmentCount) / (24 * 3600);
}

}  // namespace sdl_preview_progress
