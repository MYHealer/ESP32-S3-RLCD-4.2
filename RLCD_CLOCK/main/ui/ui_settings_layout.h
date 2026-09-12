// 统一定义固件设置页与 SDL 预览共用的固定布局坐标。
#pragma once

#include <stddef.h>

namespace ui_settings_layout {

struct GridCell {
    int x;
    int y;
};

inline constexpr int kSettingsPrimaryX = 12;
inline constexpr int kSettingsPrimaryW = 112;
inline constexpr int kSettingsSecondaryX = 150;
inline constexpr int kSettingsSecondaryW = 228;
inline constexpr int kSettingsSecondaryH = 30;
inline constexpr int kSettingsSwitchDotX = 362;
inline constexpr int kSettingsSwitchDotYOffset = 8;
inline constexpr int kSettingsSwitchDotSize = 12;
inline constexpr int kSettingsListRowY[] = {66, 105, 144, 183, 222, 222, 222, 222};
inline constexpr int kSettingsGridRowY[] = {66, 105, 144, 183};
inline constexpr size_t kSettingsListRowCount =
    sizeof(kSettingsListRowY) / sizeof(kSettingsListRowY[0]);
inline constexpr size_t kSettingsGridRowCount =
    sizeof(kSettingsGridRowY) / sizeof(kSettingsGridRowY[0]);
inline constexpr int kSettingsGridColumns = 2;
inline constexpr int kSettingsGridLeftX = 150;
inline constexpr int kSettingsGridRightX = 267;
inline constexpr int kSettingsGridColW = 111;
inline constexpr int kSettingsGridSwitchTextXOffset = 83;
inline constexpr int kSettingsGridSwitchTextYOffset = 7;
inline constexpr int kSettingsGridSwitchDotXOffset = 93;
inline constexpr int kSettingsGridSwitchDotYOffset = 9;
inline constexpr int kSettingsGridLabelPadding = 4;
inline constexpr int kSettingsGridSwitchLabelLeftPadding = 0;
inline constexpr int kSettingsGridSwitchLabelRightPadding = 22;
inline constexpr int kSettingsGridSwitchLabelDotGap = 4;
inline constexpr int kSettingsSystemLongItemY = 144;
inline constexpr int kSettingsDisplayLongItemY = 183;

static_assert(kSettingsGridSwitchLabelRightPadding >=
                  kSettingsGridColW - kSettingsGridSwitchDotXOffset +
                      kSettingsGridSwitchLabelDotGap,
              "switch labels must reserve a separate region for the status dot");

inline constexpr int kSettingsGridCapacity =
    static_cast<int>(kSettingsGridRowCount) * kSettingsGridColumns;

constexpr bool settings_grid_index_valid(int index)
{
    return index >= 0 && index < kSettingsGridCapacity;
}

constexpr GridCell settings_grid_cell(int index)
{
    if (!settings_grid_index_valid(index)) {
        return {-1, -1};
    }
    const int column = index % kSettingsGridColumns;
    const int row = index / kSettingsGridColumns;
    return {
        column == 0 ? kSettingsGridLeftX : kSettingsGridRightX,
        kSettingsGridRowY[row],
    };
}

} // namespace ui_settings_layout
