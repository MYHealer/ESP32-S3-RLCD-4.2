// 统一定义应用有效年份与 C struct tm 自然年月换算契约。
#pragma once

inline constexpr int kTmYearOffset = 1900;
inline constexpr int kTmMonthOffset = 1;
inline constexpr int kMinValidYear = 2024;
inline constexpr int kMaxValidYear = 2035;

static_assert(kTmYearOffset == 1900,
              "struct tm year offset must stay 1900");
static_assert(kTmMonthOffset == 1,
              "struct tm month offset must stay 1");
static_assert(kMinValidYear <= kMaxValidYear,
              "valid year range must stay ordered");
static_assert(kMinValidYear >= kTmYearOffset,
              "valid years must use natural calendar numbering");
