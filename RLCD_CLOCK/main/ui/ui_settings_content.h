// 声明设置页二级菜单动态文案生成接口。
#pragma once

#include "ui_settings_contract.h"

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t kSettingsSecondaryTextSize = 56;

struct SettingsSecondaryStateSnapshot {
    bool hourly_chime_enabled;
    bool all_day_chime_enabled;
    uint8_t volume_percent;
    uint8_t sound_index;
    uint8_t work_page_enabled_mask;
    bool alarm_enabled;
    uint8_t alarm_hour;
    uint8_t alarm_minute;
    bool xiaozhi_auto_return_enabled;
};

bool settings_secondary_index_valid(int index);
void set_secondary_text(char items[][kSettingsSecondaryTextSize],
                        int index,
                        const char *text);
void format_secondary_text(char items[][kSettingsSecondaryTextSize],
                           int index,
                           const char *format,
                           ...);
void populate_settings_secondary_items(
    int primary,
    const SettingsSecondaryStateSnapshot &state,
    char secondary_items[][kSettingsSecondaryTextSize]);
