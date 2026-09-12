// 定义所有工作页与 SDL 预览共用的基础布局尺寸。
#pragma once

namespace ui_work_page_layout {

inline constexpr int kTopSeparatorX = 18;
inline constexpr int kTopSeparatorY = 54;
inline constexpr int kTopSeparatorWidth = 364;
inline constexpr int kTopSeparatorHeight = 4;

static_assert(kTopSeparatorX >= 0 && kTopSeparatorY >= 0,
              "work-page top separator origin must be non-negative");
static_assert(kTopSeparatorWidth > 0 && kTopSeparatorHeight > 0,
              "work-page top separator size must be positive");

} // namespace ui_work_page_layout
