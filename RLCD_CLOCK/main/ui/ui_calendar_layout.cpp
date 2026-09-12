// 实现日历日期到固定五行视口的映射，不包含日期算法或 LVGL 绘制。
#include "ui_calendar_layout.h"

#include "app_text_format.h"
#include "app_time_constants.h"

namespace {

constexpr int kCalendarVisibleCellLimit =
    kCalendarWeekdayCount * kCalendarVisibleRowCount;
constexpr int kMonthsPerYear = 12;

} // namespace

static_assert(kCalendarWeekdayCount > 0, "calendar weekday count must be positive");
static_assert(kCalendarVisibleRowCount > 0, "calendar visible row count must be positive");

bool calculate_calendar_month_layout(int first_weekday,
                                     int day_count,
                                     int today,
                                     CalendarMonthLayout *out)
{
    if (!out || first_weekday < 0 || first_weekday >= kCalendarWeekdayCount ||
        day_count <= 0 || today <= 0 || today > day_count) {
        return false;
    }

    const int today_index = first_weekday + today - 1;
    const int today_row = today_index / kCalendarWeekdayCount;
    const bool needs_sixth_row = first_weekday + day_count > kCalendarVisibleCellLimit;
    out->first_weekday = first_weekday;
    out->day_count = day_count;
    out->row_offset = needs_sixth_row && today_row >= kCalendarVisibleRowCount ? 1 : 0;
    return true;
}

bool calendar_day_display_position(const CalendarMonthLayout &layout,
                                   int day,
                                   int *display_row,
                                   int *column)
{
    if (display_row) {
        *display_row = -1;
    }
    if (column) {
        *column = -1;
    }
    if (!display_row || !column || layout.first_weekday < 0 ||
        layout.first_weekday >= kCalendarWeekdayCount || layout.day_count <= 0 ||
        day <= 0 || day > layout.day_count || layout.row_offset < 0) {
        return false;
    }

    const int index = layout.first_weekday + day - 1;
    const int row = index / kCalendarWeekdayCount - layout.row_offset;
    if (row < 0 || row >= kCalendarVisibleRowCount) {
        return false;
    }
    *display_row = row;
    *column = index % kCalendarWeekdayCount;
    return true;
}

int calendar_month_key(const struct tm &local)
{
    return (local.tm_year + kTmYearOffset) * kMonthsPerYear + local.tm_mon;
}

void format_calendar_day_text(char *out, size_t out_len, int day)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    if (day < 10 && out_len >= 2) {
        out[0] = static_cast<char>('0' + day);
        out[1] = '\0';
        return;
    }
    if (out_len >= 3) {
        out[0] = static_cast<char>('0' + day / 10);
        out[1] = static_cast<char>('0' + day % 10);
        out[2] = '\0';
        return;
    }
    out[0] = '\0';
}
