// 统一定义设置菜单索引、容量和手动同步操作类型。
#pragma once

inline constexpr int kSettingsPrimaryCount = 4;
inline constexpr int kSettingsSecondaryMaxCount = 8;
inline constexpr int kSettingsLabelCount =
    kSettingsPrimaryCount + kSettingsSecondaryMaxCount;

inline constexpr int kNetworkSettingsNtpItem = 0;
inline constexpr int kNetworkSettingsWeatherItem = 1;
inline constexpr int kNetworkSettingsSayingItem = 2;
inline constexpr int kNetworkSettingsWeatherCityItem = 3;
inline constexpr int kNetworkSettingsSecondaryCount =
    kNetworkSettingsWeatherCityItem + 1;

inline constexpr int kSoundSettingsVolumeItem = 0;
inline constexpr int kSoundSettingsSoundItem = 1;
inline constexpr int kSoundSettingsHourlyItem = 2;
inline constexpr int kSoundSettingsAllDayItem = 3;
inline constexpr int kSoundSettingsSecondaryCount =
    kSoundSettingsAllDayItem + 1;

inline constexpr int kDisplaySettingsPageSwitchItem = 0;
inline constexpr int kDisplaySettingsOrderItem = 1;
inline constexpr int kDisplaySettingsXiaozhiAutoReturnItem = 2;
inline constexpr int kDisplaySettingsAlarmItem = 3;
inline constexpr int kDisplaySettingsGalleryRotationItem = 4;
inline constexpr int kDisplaySettingsSecondaryCount =
    kDisplaySettingsGalleryRotationItem + 1;
inline constexpr int kDisplaySettingsGridItemCount =
    kDisplaySettingsSecondaryCount;

inline constexpr int kSystemSettingsOfflineItem = 0;
inline constexpr int kSystemSettingsNetworkDiagItem = 1;
inline constexpr int kSystemSettingsFactoryResetItem = 2;
inline constexpr int kSystemSettingsInfoItem = 3;
inline constexpr int kSystemSettingsOtaItem = 4;
inline constexpr int kSystemSettingsGridItemCount = 4;
inline constexpr int kSystemSettingsSecondaryCount =
    kSystemSettingsOtaItem + 1;

enum SettingsSyncOp {
    kSettingsSyncNone = 0,
    kSettingsSyncNtp = 1,
    kSettingsSyncWeather = 2,
    kSettingsSyncSaying = 3,
    kSettingsSyncNetworkDiag = 4,
};

enum SettingsPrimaryMenu {
    kSettingsPrimaryNetwork = 0,
    kSettingsPrimarySound = 1,
    kSettingsPrimaryDisplay = 2,
    kSettingsPrimarySystem = 3,
};

static_assert(kSettingsPrimarySystem + 1 == kSettingsPrimaryCount,
              "settings primary count must match the final menu id");
static_assert(kDisplaySettingsXiaozhiAutoReturnItem + 1 ==
                  kDisplaySettingsAlarmItem,
              "Xiaozhi power saving must remain immediately above alarm");
static_assert(kDisplaySettingsSecondaryCount ==
                  kDisplaySettingsGalleryRotationItem + 1,
              "display settings count must include gallery rotation");
static_assert(kDisplaySettingsGridItemCount == kDisplaySettingsSecondaryCount,
              "all display settings items use the compact grid");
static_assert(kSystemSettingsGridItemCount < kSystemSettingsSecondaryCount,
              "system grid items must leave the OTA item in its long row");
