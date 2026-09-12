// 处理设置页网络、声音、显示和系统操作，不承担页面绘制。
#include "ui_settings_actions.h"

#include "active_work_page_state_internal.h"
#include "alarm_services.h"
#include "app_event_group.h"
#include "app_metadata.h"
#include "app_runtime_timing.h"
#include "audio_services.h"
#include "chime_runtime_state.h"
#include "chime_settings.h"
#include "custom_assets.h"
#include "manual_weather_city_state.h"
#include "network_config.h"
#include "device_settings_persistence.h"
#include "network_diagnostics.h"
#include "network_diagnostics_state.h"
#include "network_runtime_events.h"
#include "offline_mode_state.h"
#include "ota_services.h"
#include "pomodoro_services.h"
#include "setup_portal_control.h"
#include "ui_info_page_state_internal.h"
#include "ui_gallery_rotation_state.h"
#include "ui_settings_activity_state.h"
#include "ui_settings_confirmation_state_internal.h"
#include "ui_settings_feedback.h"
#include "ui_settings_navigation.h"
#include "ui_settings_navigation_state_internal.h"
#include "ui_text_format.h"
#include "ui_work_page_catalog.h"
#include "work_page_ids.h"
#include "xiaozhi_auto_return_state.h"

#include <cstdarg>
#include <esp_log.h>

namespace {
constexpr uint8_t kAllWorkPageMask = static_cast<uint8_t>((1U << kWorkPageCount) - 1);
constexpr int kSettingsFeedbackDefaultMs = 2500;
constexpr int kSettingsFeedbackBusyMs = 2000;
constexpr int kSettingsFeedbackSavedMs = 1800;
constexpr int kSettingsFeedbackInstructionMs = 3500;
constexpr const char *kSettingsSaveFailedFeedback = "保存失败";
constexpr const char *kSettingsOrderSavedFeedback = "页面顺序已保存";
constexpr const char *kSettingsSyncBusyFeedback = "请等待同步完成";
constexpr const char *kSettingsOfflineEnabledFeedback = "离线模式已开启";
constexpr const char *kSettingsOfflineDisabledFeedback = "离线模式已关闭";
constexpr const char *kOfflinePageUnavailableFeedback = "当前处于离线模式";
constexpr const char *kManualWeatherCityEditFeedback = "请进入配网页/小智/网页修改";
constexpr const char *kManualWeatherCityClearConfirmFeedback = "再次确认清除";
constexpr const char *kManualWeatherCityAutoFeedback = "已恢复自动定位";
constexpr const char *kManualNtpSyncFeedback = "正在同步时间...";
constexpr const char *kManualWeatherSyncFeedback = "正在同步天气...";
constexpr const char *kManualSayingSyncFeedback = "正在更新一言...";
constexpr const char *kSoundVolumeFeedbackFormat = "音量 %d%%";
constexpr const char *kSoundIndexFeedbackFormat = "声音 %d";
constexpr const char *kHourlyChimeEnabledFeedback = "整点提醒已开启";
constexpr const char *kHourlyChimeDisabledFeedback = "整点提醒已关闭";
constexpr const char *kAllDayChimeEnabledFeedback = "全天提醒已开启";
constexpr const char *kAllDayChimeDisabledFeedback = "全天提醒已关闭";
constexpr const char *kPageOrderInstructionFeedback = "BOOT交换并保存";
constexpr const char *kPageSwitchInstructionFeedback = "页面开关：BOOT切换";
constexpr const char *kLastWorkPageFeedback = "至少保留一个页面";
constexpr const char *kXiaozhiNeedsHomeFeedback = "请至少保留一个非小智页面";
constexpr const char *kXiaozhiHomeBlockedFeedback = "小智AI不能设为主页";
constexpr const char *kPomodoroRunningFeedback = "请先取消番茄钟";
constexpr const char *kXiaozhiAutoReturnEnabledFeedback = "小智节能已开启";
constexpr const char *kXiaozhiAutoReturnDisabledFeedback = "小智节能已关闭";
constexpr const char *kGalleryRotationBuiltinFeedback = "默认图片固定24h";
constexpr const char *kGalleryRotationFeedbackFormat = "图片切换 %s";
constexpr const char *kAlarmDisabledFeedback = "闹钟已关闭";
constexpr const char *kAlarmSetByXiaozhiFeedback = "请通过小智AI设置";
constexpr const char *kWorkPageFeedbackFormat = "%s%s";
constexpr const char *kWorkPageEnabledSuffix = "已开启";
constexpr const char *kWorkPageDisabledSuffix = "已关闭";
constexpr const char *kOfflineSetupConfirmFeedback = "再次确认进入配网";
constexpr const char *kSetupStartFailedFeedback = "配网启动失败";
constexpr const char *kOfflineSetupInstructionFeedback = "请完成配网后关闭";
constexpr const char *kNetworkDiagSyncFeedback = "正在网络检测...";
constexpr const char *kFactoryResetConfirmFeedback = "再次按 BOOT 确认";
constexpr const char *kFactoryResetFailedFeedback = "恢复失败";
constexpr size_t kSettingsFeedbackTextSize = 32;
#define CHIME_BOOLEAN_SETTING_LOG_FORMAT "%s %s"
#define CHIME_SETTING_ENABLED_LOG_VALUE "enabled"
#define CHIME_SETTING_DISABLED_LOG_VALUE "disabled"
#define MANUAL_WEATHER_CITY_CLEARED_SYNC_LOG "manual weather city cleared, requesting weather sync"
#define MANUAL_NTP_SYNC_REQUESTED_LOG "manual ntp sync requested"
#define MANUAL_WEATHER_SYNC_REQUESTED_LOG "manual weather sync requested"
#define MANUAL_SAYING_SYNC_REQUESTED_LOG "manual daily saying sync requested"
#define MANUAL_NETWORK_DIAG_REQUESTED_LOG "manual network diagnostics requested"
#define FACTORY_RESET_CONFIRM_REQUESTED_LOG "factory reset confirmation requested"
#define FACTORY_RESET_REQUESTED_LOG "factory reset requested from settings"
#define SYSTEM_INFO_REQUESTED_LOG "system info requested from settings"

uint8_t toggled_work_page_mask(uint8_t current_mask, int page)
{
    if (!is_valid_work_page_id(page)) {
        return static_cast<uint8_t>(current_mask & kAllWorkPageMask);
    }
    uint8_t page_mask = static_cast<uint8_t>(1U << page);
    return static_cast<uint8_t>((current_mask ^ page_mask) & kAllWorkPageMask);
}

void set_formatted_settings_feedback(const char *format, ...)
{
    if (!format) {
        set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
        return;
    }
    char feedback[kSettingsFeedbackTextSize] = {};
    va_list args;
    va_start(args, format);
    int written = vsnprintf(feedback, sizeof(feedback), format, args);
    va_end(args);
    if (ui_text::format_failed(written, sizeof(feedback))) {
        set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
        return;
    }
    set_settings_feedback(feedback, kSettingsFeedbackDefaultMs);
}

bool set_chime_setting_or_feedback(const ChimeRuntimeSnapshot &next)
{
    if (set_chime_setting(next)) {
        return true;
    }
    set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
    return false;
}

void toggle_chime_boolean_setting(
    bool ChimeRuntimeSnapshot::*field,
    const char *enabled_feedback,
    const char *disabled_feedback,
    const char *log_name)
{
    const ChimeRuntimeSnapshot previous = chime_runtime_snapshot_load();
    ChimeRuntimeSnapshot next = previous;
    next.*field = !(previous.*field);
    if (!set_chime_setting_or_feedback(next)) {
        return;
    }
    const bool enabled = next.*field;
    set_settings_feedback(enabled ? enabled_feedback : disabled_feedback,
                          kSettingsFeedbackDefaultMs);
    ESP_LOGI(TAG,
             CHIME_BOOLEAN_SETTING_LOG_FORMAT,
             log_name,
             enabled ? CHIME_SETTING_ENABLED_LOG_VALUE
                     : CHIME_SETTING_DISABLED_LOG_VALUE);
    if (enabled) {
        request_settings_confirmation_chime();
    }
}

void queue_manual_settings_sync(SettingsSyncOp op,
                                const char *feedback,
                                const char *log_message,
                                EventBits_t event_bit)
{
    begin_settings_sync(op, feedback, event_bit);
    ESP_LOGI(TAG, "%s", log_message);
}

void clear_inactive_settings_confirmation(int primary, int selected)
{
    if (!(primary == kSettingsPrimarySystem && selected == kSystemSettingsFactoryResetItem)) {
        settings_confirmation_clear(SettingsConfirmation::kFactoryReset);
    }
    if (!(primary == kSettingsPrimarySystem && selected == kSystemSettingsOfflineItem)) {
        settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
    }
    if (!(primary == kSettingsPrimaryNetwork && selected == kNetworkSettingsWeatherCityItem)) {
        settings_confirmation_clear(SettingsConfirmation::kWeatherCityClear);
    }
}

static_assert(kWorkPageCount > 0 && kWorkPageCount <= 8,
              "work page mask in settings UI is stored as uint8_t");
static_assert(kAllWorkPageMask != 0, "settings UI must have at least one work page bit");
} // namespace

namespace {
void handle_page_order_settings_action(
    SettingsNavigationSnapshot navigation)
{
    int current = valid_enabled_work_page_order_index(navigation.page_order_selection);
    int next = next_enabled_work_page_order_index(current);
    uint8_t next_order[kWorkPageCount] = {};
    if (!work_page_order_swapped_copy_preserving_home(
            current, next, next_order, sizeof(next_order))) {
        set_settings_feedback(kXiaozhiHomeBlockedFeedback, kSettingsFeedbackDefaultMs);
        return;
    }
    if (set_work_page_order_setting(next_order, sizeof(next_order))) {
        navigation.page_order_selection = next;
        settings_navigation_store(navigation);
        active_work_page_store(first_enabled_work_page());
        set_settings_feedback(kSettingsOrderSavedFeedback, kSettingsFeedbackSavedMs);
    } else {
        navigation.page_order_selection = current;
        settings_navigation_store(navigation);
        set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
    }
}

void handle_network_settings_action(int selected)
{
    if (selected == kNetworkSettingsWeatherCityItem) {
        if (!manual_weather_city_is_configured()) {
            set_settings_feedback(kManualWeatherCityEditFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        if (!settings_confirmation_pending(SettingsConfirmation::kWeatherCityClear)) {
            settings_confirmation_request(SettingsConfirmation::kWeatherCityClear);
            set_settings_feedback(kManualWeatherCityClearConfirmFeedback, kSettingsTimeoutMs);
            return;
        }
        if (!clear_manual_weather_city()) {
            set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        settings_confirmation_clear(SettingsConfirmation::kWeatherCityClear);
        if (offline_mode_enabled_load()) {
            set_settings_feedback(kManualWeatherCityAutoFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        queue_manual_settings_sync(kSettingsSyncWeather,
                                   kManualWeatherSyncFeedback,
                                   MANUAL_WEATHER_CITY_CLEARED_SYNC_LOG,
                                   kManualWeatherSyncBit);
        return;
    }
    if (offline_mode_enabled_load()) {
        set_settings_feedback(kSettingsOfflineEnabledFeedback, kSettingsFeedbackDefaultMs);
        return;
    }
    if (selected == kNetworkSettingsNtpItem) {
        queue_manual_settings_sync(kSettingsSyncNtp,
                                   kManualNtpSyncFeedback,
                                   MANUAL_NTP_SYNC_REQUESTED_LOG,
                                   kManualNtpSyncBit);
    } else if (selected == kNetworkSettingsWeatherItem) {
        queue_manual_settings_sync(kSettingsSyncWeather,
                                   kManualWeatherSyncFeedback,
                                   MANUAL_WEATHER_SYNC_REQUESTED_LOG,
                                   kManualWeatherSyncBit);
    } else if (selected == kNetworkSettingsSayingItem) {
        queue_manual_settings_sync(kSettingsSyncSaying,
                                   kManualSayingSyncFeedback,
                                   MANUAL_SAYING_SYNC_REQUESTED_LOG,
                                   kManualSayingSyncBit);
    }
}

void handle_sound_settings_action(int selected)
{
    if (selected == kSoundSettingsVolumeItem) {
        const ChimeRuntimeSnapshot previous = chime_runtime_snapshot_load();
        ChimeRuntimeSnapshot next = previous;
        next.volume_percent = static_cast<uint8_t>(
            chime_settings::next_volume_percent(previous.volume_percent));
        if (!set_chime_setting_or_feedback(next)) {
            return;
        }
        set_formatted_settings_feedback(kSoundVolumeFeedbackFormat, next.volume_percent);
        request_settings_confirmation_chime();
    } else if (selected == kSoundSettingsSoundItem) {
        const ChimeRuntimeSnapshot previous = chime_runtime_snapshot_load();
        ChimeRuntimeSnapshot next = previous;
        next.sound_index = static_cast<uint8_t>(
            (previous.sound_index + 1) % chime_settings::kSoundCount);
        if (!set_chime_setting_or_feedback(next)) {
            return;
        }
        set_formatted_settings_feedback(kSoundIndexFeedbackFormat, next.sound_index + 1);
        request_settings_confirmation_chime();
    } else if (selected == kSoundSettingsHourlyItem) {
        toggle_chime_boolean_setting(
            &ChimeRuntimeSnapshot::hourly_enabled,
            kHourlyChimeEnabledFeedback,
            kHourlyChimeDisabledFeedback,
            "hourly chime");
    } else if (selected == kSoundSettingsAllDayItem) {
        toggle_chime_boolean_setting(
            &ChimeRuntimeSnapshot::all_day,
            kAllDayChimeEnabledFeedback,
            kAllDayChimeDisabledFeedback,
            "hourly chime all-day");
    }
}

void handle_display_settings_action(
    int selected,
    SettingsNavigationSnapshot navigation)
{
    if (navigation.page_toggle_mode) {
        int page = selected;
        if (!is_valid_work_page_id(page)) {
            set_settings_feedback(kSettingsSaveFailedFeedback,
                                  kSettingsFeedbackDefaultMs);
            return;
        }
        const uint8_t previous_mask = work_page_enabled_mask_load();
        const bool page_was_enabled =
            (previous_mask & static_cast<uint8_t>(1U << page)) != 0;
        const bool page_will_be_enabled = !page_was_enabled;
        if (offline_mode_enabled_load() &&
            !page_was_enabled &&
            work_page_requires_network(page)) {
            set_settings_feedback(kOfflinePageUnavailableFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        const uint8_t next_mask = toggled_work_page_mask(previous_mask, page);
        if (next_mask == 0) {
            set_settings_feedback(kLastWorkPageFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        if (!work_page_mask_has_valid_home(next_mask)) {
            set_settings_feedback(kXiaozhiNeedsHomeFeedback, kSettingsFeedbackInstructionMs);
            return;
        }
        if (page == kWorkPageXiaozhiAI &&
            page_was_enabled &&
            pomodoro_is_running()) {
            set_settings_feedback(kPomodoroRunningFeedback, kSettingsFeedbackInstructionMs);
            return;
        }
        if (!set_work_page_enabled_mask_setting(next_mask)) {
            set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        notify_network_sync_runtime_state_changed();
        normalize_work_page_order();
        ensure_active_work_page_enabled();
        set_formatted_settings_feedback(kWorkPageFeedbackFormat,
                                        work_page_name(page),
                                        page_will_be_enabled ? kWorkPageEnabledSuffix : kWorkPageDisabledSuffix);
        return;
    }
    if (selected == kDisplaySettingsPageSwitchItem) {
        navigation.page_order_mode = false;
        navigation.page_toggle_mode = true;
        navigation.selection = 0;
        settings_navigation_store(navigation);
        set_settings_feedback(kPageSwitchInstructionFeedback, kSettingsFeedbackInstructionMs);
        return;
    }
    if (selected == kDisplaySettingsOrderItem) {
        navigation.page_toggle_mode = false;
        navigation.page_order_mode = true;
        navigation.page_order_selection = first_enabled_work_page_order_index();
        settings_navigation_store(navigation);
        set_settings_feedback(kPageOrderInstructionFeedback, kSettingsFeedbackInstructionMs);
        return;
    }
    if (selected == kDisplaySettingsAlarmItem) {
        if (!alarm_is_enabled()) {
            set_settings_feedback(kAlarmSetByXiaozhiFeedback, kSettingsFeedbackInstructionMs);
            return;
        }
        if (!alarm_disable()) {
            set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        set_settings_feedback(kAlarmDisabledFeedback, kSettingsFeedbackDefaultMs);
        return;
    }
    if (selected == kDisplaySettingsXiaozhiAutoReturnItem) {
        const bool enabled = !xiaozhi_auto_return_enabled_load();
        if (!set_xiaozhi_auto_return_setting(enabled)) {
            set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        set_settings_feedback(enabled ? kXiaozhiAutoReturnEnabledFeedback
                                      : kXiaozhiAutoReturnDisabledFeedback,
                              kSettingsFeedbackDefaultMs);
        return;
    }
    if (selected == kDisplaySettingsGalleryRotationItem) {
        if (custom_assets_gallery_count() <= 0) {
            set_settings_feedback(kGalleryRotationBuiltinFeedback,
                                  kSettingsFeedbackInstructionMs);
            return;
        }
        const uint8_t next =
            next_gallery_rotation_period(gallery_rotation_period_load());
        if (!set_gallery_rotation_period_setting(next)) {
            set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        set_formatted_settings_feedback(kGalleryRotationFeedbackFormat,
                                        gallery_rotation_period_label(next));
        return;
    }
    set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
}

void handle_system_settings_action(
    int selected,
    SettingsNavigationSnapshot navigation)
{
    if (selected == kSystemSettingsOfflineItem) {
        if (!offline_mode_enabled_load()) {
            if (!set_offline_mode_enabled(true)) {
                set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
                return;
            }
            settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
            set_settings_feedback(kSettingsOfflineEnabledFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        if (can_leave_offline_mode_without_setup()) {
            if (!set_offline_mode_enabled(false)) {
                set_settings_feedback(kSettingsSaveFailedFeedback, kSettingsFeedbackDefaultMs);
                return;
            }
            settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
            set_settings_feedback(kSettingsOfflineDisabledFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        if (!settings_confirmation_pending(SettingsConfirmation::kOfflineDisable)) {
            settings_confirmation_request(SettingsConfirmation::kOfflineDisable);
            set_settings_feedback(kOfflineSetupConfirmFeedback, kSettingsTimeoutMs);
            return;
        }
        if (!request_setup_portal_start()) {
            set_settings_feedback(kSetupStartFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
        set_settings_feedback(kOfflineSetupInstructionFeedback, kSettingsFeedbackInstructionMs);
    } else if (selected == kSystemSettingsNetworkDiagItem) {
        if (offline_mode_enabled_load()) {
            set_settings_feedback(kSettingsOfflineEnabledFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        ESP_LOGI(TAG, "%s", MANUAL_NETWORK_DIAG_REQUESTED_LOG);
        network_diag_reset();
        settings_page_clear();
        network_diag_page_request();
        navigation.focus_secondary = true;
        navigation.primary_selection = kSettingsPrimarySystem;
        navigation.selection = 0;
        settings_navigation_store(navigation);
        info_page_hold_until_store(0);
        begin_settings_sync(kSettingsSyncNetworkDiag,
                            kNetworkDiagSyncFeedback,
                            kNetworkDiagBit);
    } else if (selected == kSystemSettingsFactoryResetItem) {
        if (!settings_confirmation_pending(SettingsConfirmation::kFactoryReset)) {
            settings_confirmation_request(SettingsConfirmation::kFactoryReset);
            set_settings_feedback(kFactoryResetConfirmFeedback, kSettingsTimeoutMs);
            ESP_LOGW(TAG, "%s", FACTORY_RESET_CONFIRM_REQUESTED_LOG);
            return;
        }
        ESP_LOGW(TAG, "%s", FACTORY_RESET_REQUESTED_LOG);
        if (!clear_saved_config()) {
            set_settings_feedback(kFactoryResetFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        if (!request_setup_portal_start()) {
            set_settings_feedback(kSetupStartFailedFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        navigation.page_toggle_mode = false;
        navigation.page_order_mode = false;
        settings_navigation_store(navigation);
        settings_confirmation_clear(SettingsConfirmation::kFactoryReset);
        settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
    } else if (selected == kSystemSettingsInfoItem) {
        settings_page_clear();
        navigation.page_toggle_mode = false;
        navigation.page_order_mode = false;
        settings_navigation_store(navigation);
        settings_confirmation_clear(SettingsConfirmation::kFactoryReset);
        info_page_request(xTaskGetTickCount() + pdMS_TO_TICKS(kSettingsTimeoutMs));
        ESP_LOGI(TAG, "%s", SYSTEM_INFO_REQUESTED_LOG);
    } else if (selected == kSystemSettingsOtaItem) {
        if (offline_mode_enabled_load()) {
            set_settings_feedback(kSettingsOfflineEnabledFeedback, kSettingsFeedbackDefaultMs);
            return;
        }
        ota_handle_info_key();
    }
}
} // namespace

void handle_settings_action()
{
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    int primary = navigation.primary_selection;
    if (primary < 0 || primary >= kSettingsPrimaryCount) {
        primary = kSettingsPrimaryNetwork;
    }
    int selected = clamp_settings_selection_for_mode(primary,
                                                     navigation.selection,
                                                     navigation.page_toggle_mode);
    if (navigation.primary_selection != primary || navigation.selection != selected) {
        navigation.primary_selection = primary;
        navigation.selection = selected;
        settings_navigation_store(navigation);
    }
    settings_activity_record(xTaskGetTickCount());
    if (navigation.page_order_mode) {
        handle_page_order_settings_action(navigation);
        return;
    }
    if (!navigation.focus_secondary) {
        navigation.focus_secondary = true;
        navigation.selection = 0;
        settings_navigation_store(navigation);
        reset_settings_confirmation();
        clear_settings_feedback();
        return;
    }
    if (is_settings_sync_busy()) {
        set_settings_feedback(kSettingsSyncBusyFeedback, kSettingsFeedbackBusyMs);
        return;
    }
    clear_inactive_settings_confirmation(primary, selected);
    if (primary == kSettingsPrimaryNetwork) {
        handle_network_settings_action(selected);
    } else if (primary == kSettingsPrimarySound) {
        handle_sound_settings_action(selected);
    } else if (primary == kSettingsPrimaryDisplay) {
        handle_display_settings_action(selected, navigation);
    } else if (primary == kSettingsPrimarySystem) {
        handle_system_settings_action(selected, navigation);
    }
}
