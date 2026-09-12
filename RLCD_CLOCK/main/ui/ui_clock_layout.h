// 定义天气时钟固件与 SDL 预览共用的静态布局。
#pragma once

#include "ui_work_page_layout.h"

namespace ui_clock_layout {

inline constexpr int kClockTimeCanvasX = 18;
inline constexpr int kClockTimeCanvasY = 76;
inline constexpr int kClockTimeCanvasWidth = 292;
inline constexpr int kClockTimeCanvasHeight = 92;
inline constexpr int kClockSecondCanvasX = 320;
inline constexpr int kClockSecondCanvasY = 124;
inline constexpr int kClockSecondCanvasWidth = 60;
inline constexpr int kClockSecondCanvasHeight = 40;
inline constexpr int kClockStatusGifCanvasX = 279;
inline constexpr int kClockStatusGifCanvasY = 196;

inline constexpr int kClockDateLabelX = 198;
inline constexpr int kClockDateLabelY = 15;
inline constexpr int kClockDateLabelWidth = 182;
inline constexpr int kClockDateLabelHeight = 26;

inline constexpr int kClockAlertPillX = 64;
inline constexpr int kClockAlertPillY = 11;
inline constexpr int kClockAlertPillWidth = 128;
inline constexpr int kClockAlertPillHeight = 26;
inline constexpr int kClockAlertPillRadius = 13;
inline constexpr int kClockAlertIconX = 4;
inline constexpr int kClockAlertIconY = 4;
inline constexpr int kClockAlertLabelX = 24;
inline constexpr int kClockAlertLabelY = 4;
inline constexpr int kClockAlertLabelWidth = 94;
inline constexpr int kClockAlertLabelHeight = 18;

inline constexpr int kClockChimeStatusIconX = 64;
inline constexpr int kClockChimeStatusIconY = 15;
inline constexpr int kClockWifiStatusIconX = 90;
inline constexpr int kClockWifiStatusIconY = 15;
inline constexpr int kClockAlarmStatusIconX = 116;
inline constexpr int kClockAlarmStatusIconY = 15;

inline constexpr int kClockDividerX =
    ui_work_page_layout::kTopSeparatorX;
inline constexpr int kClockTopDividerY =
    ui_work_page_layout::kTopSeparatorY;
inline constexpr int kClockBottomDividerY = 184;
inline constexpr int kClockDividerWidth =
    ui_work_page_layout::kTopSeparatorWidth;
inline constexpr int kClockDividerHeight =
    ui_work_page_layout::kTopSeparatorHeight;
inline constexpr int kClockSecondProgressCanvasY = 180;
inline constexpr int kClockLowerPanelSeparatorY = 188;
inline constexpr int kClockLowerPanelSeparatorWidth = 2;
inline constexpr int kClockLowerPanelSeparatorHeight = 102;
inline constexpr int kClockLowerPanelSeparatorAX = 139;
inline constexpr int kClockLowerPanelSeparatorBX = 260;

inline constexpr int kClockWeatherCityLabelX = 14;
inline constexpr int kClockWeatherCityLabelY = 196;
inline constexpr int kClockWeatherCityLabelWidth = 76;
inline constexpr int kClockWeatherCityLabelHeight = 20;
inline constexpr int kClockWeatherIconLabelX = 91;
inline constexpr int kClockWeatherIconLabelY = 194;
inline constexpr int kClockWeatherIconLabelWidth = 34;
inline constexpr int kClockWeatherIconLabelHeight = 38;
inline constexpr int kClockWeatherInfoLabelX = 14;
inline constexpr int kClockWeatherInfoLabelY = 218;
inline constexpr int kClockWeatherInfoLabelWidth = 76;
inline constexpr int kClockWeatherInfoLabelHeight = 20;
inline constexpr int kClockWeatherMetricLabelX = 20;
inline constexpr int kClockWeatherTempLabelY = 242;
inline constexpr int kClockWeatherHumiLabelY = 264;
inline constexpr int kClockWeatherMetricLabelWidth = 68;
inline constexpr int kClockWeatherMetricLabelHeight = 20;

inline constexpr int kClockTempIconX = 152;
inline constexpr int kClockTempIconY = 214;
inline constexpr int kClockHumiIconX = 154;
inline constexpr int kClockHumiIconY = 244;
inline constexpr int kClockLocalMetricLabelX = 174;
inline constexpr int kClockLocalTempLabelY = 214;
inline constexpr int kClockLocalHumiLabelY = 246;
inline constexpr int kClockLocalMetricLabelWidth = 62;
inline constexpr int kClockLocalMetricLabelHeight = 28;
inline constexpr int kClockTrendCanvasX = 239;
inline constexpr int kClockTempTrendCanvasY = 215;
inline constexpr int kClockHumiTrendCanvasY = 248;
inline constexpr int kClockLowBatteryIconX = 156;
inline constexpr int kClockLowBatteryIconY = 214;

static_assert(kClockTimeCanvasX >= 0 && kClockTimeCanvasY >= 0 &&
                  kClockTimeCanvasWidth > 0 &&
                  kClockTimeCanvasHeight > 0,
              "clock time canvas must have a valid rectangle");
static_assert(kClockSecondCanvasX >= 0 && kClockSecondCanvasY >= 0 &&
                  kClockSecondCanvasWidth > 0 &&
                  kClockSecondCanvasHeight > 0,
              "clock second canvas must have a valid rectangle");
static_assert(kClockAlertIconX >= 0 && kClockAlertIconY >= 0 &&
                  kClockAlertLabelX >= 0 && kClockAlertLabelY >= 0,
              "clock alert children must have non-negative origins");
static_assert(kClockAlertLabelX + kClockAlertLabelWidth <=
                  kClockAlertPillWidth &&
                  kClockAlertLabelY + kClockAlertLabelHeight <=
                      kClockAlertPillHeight,
              "clock alert label must fit the alert pill");
static_assert(kClockBottomDividerY >
                  kClockTopDividerY + kClockDividerHeight,
              "clock body dividers must not overlap");
static_assert(kClockLowerPanelSeparatorY +
                      kClockLowerPanelSeparatorHeight <=
                  300,
              "clock lower-panel separators must fit the display");
static_assert(kClockWeatherTempLabelY <
                  kClockWeatherHumiLabelY,
              "clock weather metrics must keep temperature above humidity");
static_assert(kClockLocalTempLabelY <
                  kClockLocalHumiLabelY,
              "clock local metrics must keep temperature above humidity");

} // namespace ui_clock_layout
