// 提供当月日历固定五行视口和第六行动态上移的纯布局计算。
#pragma once

#include <stddef.h>
#include <time.h>

inline constexpr int kCalendarWeekdayCount = 7;
inline constexpr int kCalendarVisibleRowCount = 5;

namespace ui_calendar_view_layout {

inline constexpr int kCanvasWidth = 364;
inline constexpr int kCanvasHeight = 232;
inline constexpr int kCanvasX = 18;
inline constexpr int kCanvasY = 63;
inline constexpr int kSundayColumn = 0;
inline constexpr int kSaturdayColumn = kCalendarWeekdayCount - 1;
inline constexpr int kHeaderY = 2;
inline constexpr int kHeaderHeight = 18;
inline constexpr int kCellY = 27;
inline constexpr int kCellWidth = 52;
inline constexpr int kCellHeight = 41;
inline constexpr int kGridX = 0;
inline constexpr int kDottedFillYStep = 3;
inline constexpr int kDottedFillXStep = 4;
inline constexpr int kTodayInsetX = 4;
inline constexpr int kTodayInsetTop = 3;
inline constexpr int kTodayInsetWidth = 8;
inline constexpr int kTodayInsetHeight = 5;
inline constexpr int kTodayRadius = 5;
inline constexpr int kDayTextXInset = 2;
inline constexpr int kDayTextY = 2;
inline constexpr int kDayTextWidthInset = 4;
inline constexpr int kDayTextHeight = 14;
inline constexpr int kSubTextY = 20;
inline constexpr int kSubTextHeight = 12;
inline constexpr int kDayTextSize = 4;
inline constexpr const char *kWeekdays[kCalendarWeekdayCount] = {
    "日", "一", "二", "三", "四", "五", "六",
};

static_assert(kCanvasWidth > 0 && kCanvasHeight > 0,
              "calendar canvas size must be positive");
static_assert(kGridX + kCellWidth * kCalendarWeekdayCount == kCanvasWidth,
              "calendar columns must fill canvas width");
static_assert(kCellY + (kCalendarVisibleRowCount - 1) * kCellHeight +
                      kSubTextY + kSubTextHeight <=
                  kCanvasHeight,
              "calendar last visible row text must fit canvas height");
static_assert(kCellY + (kCalendarVisibleRowCount - 1) * kCellHeight +
                      kTodayInsetTop + kCellHeight - kTodayInsetHeight <=
                  kCanvasHeight,
              "calendar last visible row highlight must fit canvas height");
static_assert(kTodayRadius > 0 &&
                  kTodayRadius * 2 <= kCellWidth - kTodayInsetWidth &&
                  kTodayRadius * 2 <= kCellHeight - kTodayInsetHeight,
              "calendar today radius must fit highlight box");

} // namespace ui_calendar_view_layout

struct CalendarMonthLayout {
    int first_weekday = 0;
    int day_count = 0;
    int row_offset = 0;
};

bool calculate_calendar_month_layout(int first_weekday,
                                     int day_count,
                                     int today,
                                     CalendarMonthLayout *out);

bool calendar_day_display_position(const CalendarMonthLayout &layout,
                                   int day,
                                   int *display_row,
                                   int *column);
int calendar_month_key(const struct tm &local);
void format_calendar_day_text(char *out, size_t out_len, int day);
