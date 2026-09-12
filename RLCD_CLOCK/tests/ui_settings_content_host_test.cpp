// 验证设置页四类二级菜单文案及安全索引边界。
#include "ui_settings_content.h"

#include "manual_weather_city_state.h"
#include "offline_mode_state.h"
#include "offline_mode_state_internal.h"
#include "ui_settings_confirmation_state_internal.h"
#include "ui_gallery_rotation_state_internal.h"
#include "weather_city_contract.h"

#include <assert.h>
#include <string.h>

namespace {
char s_manual_weather_city[kManualWeatherCityLen] = {};
int s_custom_gallery_count = 0;

void expect_text(char items[][kSettingsSecondaryTextSize], int index, const char *expected)
{
    assert(strcmp(items[index], expected) == 0);
}
} // namespace

bool manual_weather_city_snapshot(char *out, size_t out_len)
{
    if (!out || out_len < sizeof(s_manual_weather_city)) {
        return false;
    }
    memcpy(out, s_manual_weather_city, sizeof(s_manual_weather_city));
    return s_manual_weather_city[0] != '\0';
}

void manual_weather_city_store(const char *city)
{
    strlcpy(s_manual_weather_city, city ? city : "", sizeof(s_manual_weather_city));
}

bool manual_weather_city_is_configured()
{
    return s_manual_weather_city[0] != '\0';
}

int custom_assets_gallery_count()
{
    return s_custom_gallery_count;
}

int main()
{
    static_assert(kDisplaySettingsXiaozhiAutoReturnItem == 2,
                  "Xiaozhi power saving must use the left cell of the second row");
    static_assert(kDisplaySettingsAlarmItem == 3,
                  "alarm must use the right cell of the second row");
    SettingsSecondaryStateSnapshot state = {};
    state.volume_percent = 60;
    state.sound_index = 2;
    char items[kSettingsSecondaryMaxCount][kSettingsSecondaryTextSize] = {};

    assert(!settings_secondary_index_valid(-1));
    assert(settings_secondary_index_valid(0));
    assert(settings_secondary_index_valid(kSettingsSecondaryMaxCount - 1));
    assert(!settings_secondary_index_valid(kSettingsSecondaryMaxCount));

    populate_settings_secondary_items(kSettingsPrimaryNetwork, state, items);
    expect_text(items, kNetworkSettingsNtpItem, "同步时间");
    expect_text(items, kNetworkSettingsWeatherItem, "同步天气");
    expect_text(items, kNetworkSettingsSayingItem, "更新一言");
    expect_text(items, kNetworkSettingsWeatherCityItem, "天气城市 自动");

    manual_weather_city_store("杭州");
    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimaryNetwork, state, items);
    expect_text(items, kNetworkSettingsWeatherCityItem, "天气城市 杭州");

    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimarySound, state, items);
    expect_text(items, kSoundSettingsVolumeItem, "音量 60%");
    expect_text(items, kSoundSettingsSoundItem, "声音选择 3");
    expect_text(items, kSoundSettingsHourlyItem, "整点提醒 7:00 - 22:00");
    expect_text(items, kSoundSettingsAllDayItem, "全天提醒 0:00 - 24:00");

    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimaryDisplay, state, items);
    expect_text(items, kDisplaySettingsPageSwitchItem, "页面开关");
    expect_text(items, kDisplaySettingsOrderItem, "页面顺序");
    expect_text(items, kDisplaySettingsXiaozhiAutoReturnItem, "小智节能");
    expect_text(items, kDisplaySettingsAlarmItem, "闹钟 --:--");
    expect_text(items, kDisplaySettingsGalleryRotationItem, "图片切换 24h");

    gallery_rotation_period_store(kGalleryRotation30Minutes);
    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimaryDisplay, state, items);
    expect_text(items, kDisplaySettingsGalleryRotationItem, "图片切换 24h");

    s_custom_gallery_count = 3;
    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimaryDisplay, state, items);
    expect_text(items, kDisplaySettingsGalleryRotationItem, "图片切换 30m");

    state.alarm_enabled = true;
    state.alarm_hour = 6;
    state.alarm_minute = 30;
    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimaryDisplay, state, items);
    expect_text(items, kDisplaySettingsAlarmItem, "闹钟 06:30");

    offline_mode_enabled_store(true);
    settings_confirmation_request(SettingsConfirmation::kFactoryReset);
    memset(items, 0, sizeof(items));
    populate_settings_secondary_items(kSettingsPrimarySystem, state, items);
    expect_text(items, kSystemSettingsOfflineItem, "离线模式 开");
    expect_text(items, kSystemSettingsNetworkDiagItem, "网络检测");
    expect_text(items, kSystemSettingsFactoryResetItem, "确认恢复");
    expect_text(items, kSystemSettingsInfoItem, "关于本机");
    expect_text(items, kSystemSettingsOtaItem, "检查更新");

    return 0;
}
