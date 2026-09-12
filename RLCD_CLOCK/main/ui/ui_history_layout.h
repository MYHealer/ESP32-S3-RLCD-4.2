// 统一温湿历史页在固件与 SDL 预览中使用的静态布局契约。
#pragma once

namespace ui_history_layout {

inline constexpr int kDisplayedWindowHours = 24;
inline constexpr int kDisplayedSecondsPerHour = 60 * 60;
inline constexpr int kCanvasWidth = 364;
inline constexpr int kCanvasHeight = 190;
inline constexpr int kCanvasX = 18;
inline constexpr int kCanvasY = 82;

inline constexpr int kTitleX = 24;
inline constexpr int kTempTitleY = 67;
inline constexpr int kHumiTitleY = 172;
inline constexpr int kTitleWidth = 80;
inline constexpr int kTitleHeight = 24;
inline constexpr char kTempTitle[] = "温度";
inline constexpr char kHumiTitle[] = "湿度";

inline constexpr int kPlotX = 34;
inline constexpr int kTempPlotY = 10;
inline constexpr int kHumiPlotY = 112;
inline constexpr int kPlotWidth = 276;
inline constexpr int kPlotHeight = 62;
inline constexpr int kGridLineCount = 4;
inline constexpr int kGridIntervalCount = kGridLineCount - 1;
inline constexpr int kPointRadius = 3;

inline constexpr int kAxisTickCount = 5;
inline constexpr int kTimeLabelWidth = 48;
inline constexpr int kTimeLabelHeight = 18;
inline constexpr int kTimeLabelY = 274;
inline constexpr int kTimeLabelCenterX[kAxisTickCount] = {42, 110, 178, 246, 314};
inline constexpr int kAxisTickHours[kAxisTickCount] = {0, 6, 12, 18, 24};
inline constexpr char kTimePlaceholder[] = "--:--";

inline constexpr int kAxisValueCount = 3;
inline constexpr int kAxisLabelX = 332;
inline constexpr int kAxisLabelWidth = 56;
inline constexpr int kAxisLabelHeight = 18;
inline constexpr int kTempAxisLabelY = 84;
inline constexpr int kHumiAxisLabelY = 186;
inline constexpr int kAxisLabelRowGap = 30;
inline constexpr char kAxisPlaceholder[] = "--";

inline constexpr int kBadgeWidth = 40;
inline constexpr int kBadgeHeight = 16;
inline constexpr int kBadgeRadius = 6;
inline constexpr int kBadgeHorizontalPad = 3;
inline constexpr int kBadgePointGap = 4;

static_assert(kCanvasWidth > 0 && kCanvasHeight > 0,
              "history canvas dimensions must be positive");
static_assert(kTitleWidth > 0 && kTitleHeight > 0,
              "history title size must be positive");
static_assert(kTitleX >= 0 && kTitleX + kTitleWidth <= kCanvasWidth,
              "history titles must fit canvas width");
static_assert(kPlotWidth > 0 && kPlotHeight > 0,
              "history plot dimensions must be positive");
static_assert(kPlotX >= 0 && kPlotX + kPlotWidth <= kCanvasWidth,
              "history plot width must fit canvas");
static_assert(kTempPlotY >= 0 && kTempPlotY + kPlotHeight <= kCanvasHeight,
              "history temperature plot must fit canvas");
static_assert(kHumiPlotY >= 0 && kHumiPlotY + kPlotHeight <= kCanvasHeight,
              "history humidity plot must fit canvas");
static_assert(kAxisTickCount > 0 && kAxisValueCount > 0,
              "history axis counts must be positive");
static_assert(kAxisTickHours[0] == 0 &&
                  kAxisTickHours[kAxisTickCount - 1] == kDisplayedWindowHours,
              "history axis ticks must span the full display window");
static_assert(kGridLineCount > 1 &&
                  kGridIntervalCount == kGridLineCount - 1,
              "history grid intervals must match grid lines");
static_assert(kBadgeWidth > 0 && kBadgeHeight > 0 &&
                  kBadgeRadius > 0 && kPointRadius > 0,
              "history badge and point dimensions must be positive");

} // namespace ui_history_layout
