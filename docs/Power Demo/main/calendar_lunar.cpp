// 计算公历、农历、节日和节气等日历页面数据。
#include "calendar_lunar.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

template <typename T, size_t N>
constexpr size_t array_count(const T (&)[N])
{
    return N;
}

constexpr bool format_failed(int written, size_t out_len)
{
    return written < 0 || static_cast<size_t>(written) >= out_len;
}

constexpr int kTmYearOffset = 1900;
constexpr int kTmMonthOffset = 1;
constexpr int kMinValidYear = 2024;
constexpr int kMaxValidYear = 2035;

} // namespace

struct LunarYearInfo {
    int year;
    uint32_t data;
};

struct CalendarFestivalRule {
    int month;
    int day;
    const char *name;
};

static constexpr LunarYearInfo kLunarYears[] = {
    {2023, 0x05b52},
    {2024, 0x04b60},
    {2025, 0x0a6e6},
    {2026, 0x0a4e0},
    {2027, 0x0d260},
    {2028, 0x0ea65},
    {2029, 0x0d530},
    {2030, 0x05aa0},
    {2031, 0x076a3},
    {2032, 0x096d0},
    {2033, 0x04afb},
    {2034, 0x04ad0},
    {2035, 0x0a4d0},
};

constexpr bool lunar_year_table_contiguous()
{
    for (size_t i = 1; i < array_count(kLunarYears); ++i) {
        if (kLunarYears[i].year != kLunarYears[i - 1].year + 1) {
            return false;
        }
    }
    return true;
}

static const char *const kLunarDayNames[] = {
    "",
    "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十",
};

static const char *const kLunarMonthNames[] = {
    "",
    "正月", "二月", "三月", "四月", "五月", "六月",
    "七月", "八月", "九月", "十月", "冬月", "腊月",
};

static const char *const kSolarTermNames[] = {
    "小寒", "大寒", "立春", "雨水", "惊蛰", "春分",
    "清明", "谷雨", "立夏", "小满", "芒种", "夏至",
    "小暑", "大暑", "立秋", "处暑", "白露", "秋分",
    "寒露", "霜降", "立冬", "小雪", "大雪", "冬至",
};

static constexpr int kFirstGregorianMonth = 1;
static constexpr int kLastGregorianMonth = 12;
static constexpr int kFebruaryMonth = 2;
static constexpr int kCommonFebruaryDays = 28;
static constexpr int kLeapFebruaryDays = 29;
static constexpr int kFallbackGregorianMonthDays = 30;
static constexpr int kLeapYearCycle4 = 4;
static constexpr int kLeapYearCycle100 = 100;
static constexpr int kLeapYearCycle400 = 400;
static constexpr int kFirstLunarMonth = 1;
static constexpr int kLastLunarMonth = 12;
static constexpr int kFirstLunarDay = 1;
static constexpr int kLunarSmallMonthDays = 29;
static constexpr int kLunarLargeMonthDays = 30;
static constexpr int kLunarYearBaseDays = kLastLunarMonth * kLunarSmallMonthDays;
static constexpr uint32_t kLunarLeapMonthMask = 0x0f;
static constexpr uint32_t kLunarLeapMonthDaysMask = 0x10000;
static constexpr uint32_t kLunarMonthDaysBaseMask = 0x10000;
static constexpr int kLunarYearDaysFirstMask = 0x8000;
static constexpr int kLunarYearDaysLastMask = 0x8;
static constexpr int kLunarBaseYear = 2023;
static constexpr int kLunarBaseMonth = 1;
static constexpr int kLunarBaseDay = 22;
static constexpr int kSolarTermsPerMonth = 2;
static constexpr int kSolarTermsPerYear = kLastGregorianMonth * kSolarTermsPerMonth;
static constexpr const char *kCalendarLunarPlaceholder = "--";
static constexpr const char *kLunarMonthDisplayFormat = "%s%s";

static const int kGregorianMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static_assert(array_count(kGregorianMonthDays) == kLastGregorianMonth,
              "gregorian month days must cover January through December");

// 香港天文台公历与农历日期对照表中的节气日号，按小寒至冬至排列。
static constexpr uint8_t kSolarTermDays[][kSolarTermsPerYear] = {
    {6, 20, 4, 19, 5, 20, 4, 19, 5, 20, 5, 21, 6, 22, 7, 22, 7, 22, 8, 23, 7, 22, 6, 21}, // 2024
    {5, 20, 3, 18, 5, 20, 4, 20, 5, 21, 5, 21, 7, 22, 7, 23, 7, 23, 8, 23, 7, 22, 7, 21}, // 2025
    {5, 20, 4, 18, 5, 20, 5, 20, 5, 21, 5, 21, 7, 23, 7, 23, 7, 23, 8, 23, 7, 22, 7, 22}, // 2026
    {5, 20, 4, 19, 6, 21, 5, 20, 6, 21, 6, 21, 7, 23, 8, 23, 8, 23, 8, 23, 7, 22, 7, 22}, // 2027
    {6, 20, 4, 19, 5, 20, 4, 19, 5, 20, 5, 21, 6, 22, 7, 22, 7, 22, 8, 23, 7, 22, 6, 21}, // 2028
    {5, 20, 3, 18, 5, 20, 4, 20, 5, 21, 5, 21, 7, 22, 7, 23, 7, 23, 8, 23, 7, 22, 7, 21}, // 2029
    {5, 20, 4, 18, 5, 20, 5, 20, 5, 21, 5, 21, 7, 23, 7, 23, 7, 23, 8, 23, 7, 22, 7, 22}, // 2030
    {5, 20, 4, 19, 6, 21, 5, 20, 6, 21, 6, 21, 7, 23, 8, 23, 8, 23, 8, 23, 7, 22, 7, 22}, // 2031
    {6, 20, 4, 19, 5, 20, 4, 19, 5, 20, 5, 21, 6, 22, 7, 22, 7, 22, 8, 23, 7, 22, 6, 21}, // 2032
    {5, 20, 3, 18, 5, 20, 4, 20, 5, 21, 5, 21, 7, 22, 7, 23, 7, 23, 8, 23, 7, 22, 7, 21}, // 2033
    {5, 20, 4, 18, 5, 20, 5, 20, 5, 21, 5, 21, 7, 23, 7, 23, 7, 23, 8, 23, 7, 22, 7, 22}, // 2034
    {5, 20, 4, 19, 6, 21, 5, 20, 5, 21, 6, 21, 7, 23, 7, 23, 8, 23, 8, 23, 7, 22, 7, 22}, // 2035
};

static const CalendarFestivalRule kGregorianFestivals[] = {
    {1, 1, "元旦"},
    {2, 14, "情人节"},
    {3, 8, "妇女节"},
    {5, 1, "劳动节"},
    {6, 1, "儿童节"},
    {9, 10, "教师节"},
    {10, 1, "国庆"},
    {12, 25, "圣诞"},
};

static const CalendarFestivalRule kLunarFestivals[] = {
    {1, 1, "春节"},
    {1, 15, "元宵"},
    {5, 5, "端午"},
    {7, 7, "七夕"},
    {8, 15, "中秋"},
    {9, 9, "重阳"},
    {12, 8, "腊八"},
};
static_assert(array_count(kLunarYears) > 0, "lunar year table must not be empty");
static_assert(kLunarYears[0].year <= kMinValidYear &&
                  kLunarYears[array_count(kLunarYears) - 1].year >= kMaxValidYear,
              "lunar year table must cover the supported calendar range");
static_assert(lunar_year_table_contiguous(), "lunar year table must stay contiguous");
static_assert(array_count(kLunarDayNames) == static_cast<size_t>(kLunarLargeMonthDays + 1),
              "lunar day names must cover day zero through day thirty");
static_assert(array_count(kLunarMonthNames) == static_cast<size_t>(kLastLunarMonth + 1),
              "lunar month names must cover month zero through month twelve");
static_assert(array_count(kSolarTermNames) == kLastGregorianMonth * kSolarTermsPerMonth,
              "solar term names must cover two terms per month");
static_assert(array_count(kSolarTermDays) ==
                  static_cast<size_t>(kMaxValidYear - kMinValidYear + 1),
              "solar term table must cover the supported calendar range");
static_assert(array_count(kSolarTermDays[0]) == kSolarTermsPerYear,
              "solar term table must cover all terms in each year");

static int days_from_civil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
}

static const LunarYearInfo *find_lunar_year(int year)
{
    for (const auto &item : kLunarYears) {
        if (item.year == year) {
            return &item;
        }
    }
    return nullptr;
}

static bool is_gregorian_leap_year(int year)
{
    return (year % kLeapYearCycle4 == 0 && year % kLeapYearCycle100 != 0) ||
           (year % kLeapYearCycle400 == 0);
}

static int leap_month(uint32_t data)
{
    return (int)(data & kLunarLeapMonthMask);
}

static int leap_month_days(uint32_t data)
{
    if (leap_month(data) == 0) {
        return 0;
    }
    return (data & kLunarLeapMonthDaysMask) ? kLunarLargeMonthDays : kLunarSmallMonthDays;
}

static int lunar_month_days(uint32_t data, int month)
{
    return (data & (kLunarMonthDaysBaseMask >> month)) ? kLunarLargeMonthDays : kLunarSmallMonthDays;
}

static int lunar_year_days(uint32_t data)
{
    int days = kLunarYearBaseDays;
    for (int mask = kLunarYearDaysFirstMask; mask > kLunarYearDaysLastMask; mask >>= 1) {
        if (data & mask) {
            ++days;
        }
    }
    return days + leap_month_days(data);
}

int calendar_days_in_month(int year, int month)
{
    if (month < kFirstGregorianMonth || month > kLastGregorianMonth) {
        return kFallbackGregorianMonthDays;
    }
    if (month == kFebruaryMonth) {
        return is_gregorian_leap_year(year) ? kLeapFebruaryDays : kCommonFebruaryDays;
    }
    return kGregorianMonthDays[month - kFirstGregorianMonth];
}

int calendar_first_weekday(int year, int month)
{
    int days = days_from_civil(year, (unsigned)month, 1);
    int weekday = (days + 4) % 7;
    return weekday < 0 ? weekday + 7 : weekday;
}

static bool lunar_from_date(int year, int month, int day, CalendarDayInfo *info)
{
    int offset = days_from_civil(year, (unsigned)month, (unsigned)day) -
                 days_from_civil(kLunarBaseYear, kLunarBaseMonth, kLunarBaseDay);
    if (offset < 0) {
        return false;
    }

    int lunar_year = kLunarBaseYear;
    const LunarYearInfo *year_info = find_lunar_year(lunar_year);
    while (year_info) {
        int days = lunar_year_days(year_info->data);
        if (offset < days) {
            break;
        }
        offset -= days;
        ++lunar_year;
        year_info = find_lunar_year(lunar_year);
    }
    if (!year_info) {
        return false;
    }

    int lunar_month = kFirstLunarMonth;
    bool is_leap = false;
    int leap = leap_month(year_info->data);
    for (;;) {
        int days = is_leap ? leap_month_days(year_info->data) : lunar_month_days(year_info->data, lunar_month);
        if (offset < days) {
            break;
        }
        offset -= days;
        if (leap == lunar_month && !is_leap) {
            is_leap = true;
        } else {
            is_leap = false;
            ++lunar_month;
        }
        if (lunar_month > kLastLunarMonth) {
            return false;
        }
    }

    info->lunar_year = lunar_year;
    info->lunar_month = lunar_month;
    info->lunar_day = offset + 1;
    info->lunar_leap = is_leap;
    return true;
}

static const char *gregorian_festival(int month, int day)
{
    for (const auto &festival : kGregorianFestivals) {
        if (month == festival.month && day == festival.day) {
            return festival.name;
        }
    }
    return nullptr;
}

static const char *lunar_festival(int lunar_month, int lunar_day)
{
    for (const auto &festival : kLunarFestivals) {
        if (lunar_month == festival.month && lunar_day == festival.day) {
            return festival.name;
        }
    }
    return nullptr;
}

static void set_calendar_subtext(CalendarDayInfo *info, const char *text)
{
    strlcpy(info->subtext, text ? text : kCalendarLunarPlaceholder, sizeof(info->subtext));
}

static void set_calendar_lunar_month_subtext(CalendarDayInfo *info)
{
    int written = snprintf(info->subtext, sizeof(info->subtext), kLunarMonthDisplayFormat,
                           info->lunar_leap ? "闰" : "",
                           kLunarMonthNames[info->lunar_month]);
    if (format_failed(written, sizeof(info->subtext))) {
        set_calendar_subtext(info, nullptr);
    }
}

static const char *solar_term(int year, int month, int day)
{
    if (year < kMinValidYear || year > kMaxValidYear ||
        month < kFirstGregorianMonth || month > kLastGregorianMonth) {
        return nullptr;
    }
    int first = (month - kFirstGregorianMonth) * kSolarTermsPerMonth;
    const uint8_t *year_days = kSolarTermDays[year - kMinValidYear];
    for (int i = 0; i < kSolarTermsPerMonth; ++i) {
        int term = first + i;
        if (year_days[term] == day) {
            return kSolarTermNames[term];
        }
    }
    return nullptr;
}

bool calendar_day_info(const struct tm &local, CalendarDayInfo *info)
{
    if (!info) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->year = local.tm_year + kTmYearOffset;
    info->month = local.tm_mon + kTmMonthOffset;
    info->day = local.tm_mday;
    if (info->year < kMinValidYear || info->year > kMaxValidYear) {
        set_calendar_subtext(info, nullptr);
        return false;
    }

    bool lunar_ok = lunar_from_date(info->year, info->month, info->day, info);
    const char *text = gregorian_festival(info->month, info->day);
    if (!text) {
        text = solar_term(info->year, info->month, info->day);
    }
    if (!text && lunar_ok && !info->lunar_leap) {
        text = lunar_festival(info->lunar_month, info->lunar_day);
    }
    if (!text && lunar_ok) {
        if (info->lunar_day == kFirstLunarDay &&
            info->lunar_month >= kFirstLunarMonth &&
            info->lunar_month <= kLastLunarMonth) {
            set_calendar_lunar_month_subtext(info);
            return true;
        }
        if (info->lunar_day >= kFirstLunarDay && info->lunar_day <= kLunarLargeMonthDays) {
            text = kLunarDayNames[info->lunar_day];
        }
    }
    set_calendar_subtext(info, text);
    return lunar_ok;
}
