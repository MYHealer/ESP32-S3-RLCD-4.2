// 集中声明应用级 UI 超时、启动动画与可见页同步重试时序。
#pragma once

inline constexpr int kAppMsPerSecond = 1000;
inline constexpr int kAppSecondsPerMinute = 60;
inline constexpr int kAppMsPerMinute = kAppSecondsPerMinute * kAppMsPerSecond;

inline constexpr int kSettingsTimeoutMs = 30 * kAppMsPerSecond;
inline constexpr int kXiaozhiAutoReturnTimeoutMs = 5 * kAppMsPerMinute;
inline constexpr int kSettingsManualSyncTimeoutMs = kAppMsPerMinute;

inline constexpr int kWeatherClockAutoRetryMs = 2 * kAppMsPerMinute;
inline constexpr int kWeatherClockAutoSyncMaxAttempts = 3;
inline constexpr int kWeatherClockAutoBackoffMs = 30 * kAppMsPerMinute;

inline constexpr int kBootAnimRunFrameMs = 50;

static_assert(kAppMsPerMinute == 60000,
              "application minute conversion must remain stable");
static_assert(kSettingsTimeoutMs > 0 && kSettingsManualSyncTimeoutMs > 0,
              "settings timeouts must be positive");
static_assert(kXiaozhiAutoReturnTimeoutMs > kSettingsTimeoutMs,
              "Xiaozhi auto-return must exceed settings inactivity timeout");
static_assert(kWeatherClockAutoRetryMs > 0 &&
                  kWeatherClockAutoSyncMaxAttempts > 0 &&
                  kWeatherClockAutoBackoffMs > kWeatherClockAutoRetryMs,
              "visible weather retry policy must retain bounded backoff");
static_assert(kBootAnimRunFrameMs > 0,
              "boot animation frame interval must be positive");
