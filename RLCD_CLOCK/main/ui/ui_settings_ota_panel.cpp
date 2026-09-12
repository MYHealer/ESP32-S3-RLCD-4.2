// 构建并刷新设置页中的 OTA 状态、进度和操作提示。
#include "ui_settings_ota_panel.h"

#include "app_metadata.h"
#include "ota_runtime_state.h"
#include "ui_page_state.h"
#include "ui_text_format.h"
#include "ui_widgets.h"

#include <esp_log.h>

namespace {
lv_obj_t *s_settings_ota_status_label;
lv_obj_t *s_settings_ota_hint_label;
lv_obj_t *s_settings_ota_bar_frame;
lv_obj_t *s_settings_ota_bar_fill;

constexpr int kSettingsOtaBarFrameX = 164;
constexpr int kSettingsOtaBarFrameY = 203;
constexpr int kSettingsOtaBarFrameW = 200;
constexpr int kSettingsOtaBarFrameH = 9;
constexpr int kSettingsOtaBarInset = 2;
constexpr int kSettingsOtaBarFillW = kSettingsOtaBarFrameW - kSettingsOtaBarInset * 2;
constexpr int kSettingsOtaBarFillH = kSettingsOtaBarFrameH - kSettingsOtaBarInset * 2;
constexpr int kSettingsOtaProgressMax = 100;
constexpr size_t kSettingsOtaLineTextSize = 96;
constexpr size_t kSettingsOtaHintTextSize = 48;
constexpr const char *kSettingsOtaUpdatingWithSpeedFormat = "OTA %d%%  %d KB/s";
constexpr const char *kSettingsOtaUpdatingFormat = "OTA %d%%";
constexpr const char *kSettingsOtaCurrentVersionFormat = "当前版本 %s";
constexpr const char *kSettingsOtaLinePlaceholder = "OTA --";
constexpr const char *kSettingsOtaHintDownloading = "下载中，请等待";
constexpr const char *kSettingsOtaHintInstall = "BOOT安装更新";
constexpr const char *kSettingsOtaHintChecking = "正在检查，请等待";
constexpr const char *kSettingsOtaHintRebooting = "即将重启";
constexpr const char *kSettingsOtaHintRetry = "BOOT重新检查";
constexpr const char *kSettingsOtaHintCheck = "BOOT开始检查";
int settings_ota_progress_fill_width(int progress)
{
    int clamped = progress;
    if (clamped < 0) {
        clamped = 0;
    } else if (clamped > kSettingsOtaProgressMax) {
        clamped = kSettingsOtaProgressMax;
    }
    int fill_w = (kSettingsOtaBarFillW * clamped) / kSettingsOtaProgressMax;
    if (fill_w < 1) {
        fill_w = 1;
    }
    return fill_w;
}

bool set_settings_ota_fill_width(int width)
{
    if (!s_settings_ota_bar_fill || lv_obj_get_width(s_settings_ota_bar_fill) == width) {
        return false;
    }
    lv_obj_set_width(s_settings_ota_bar_fill, width);
    return true;
}

static_assert(kSettingsOtaLineTextSize > 1 && kSettingsOtaHintTextSize > 1,
              "settings OTA text buffers must fit text and NUL");
static_assert(kSettingsOtaBarInset >= 0, "settings OTA progress inset must be non-negative");
static_assert(kSettingsOtaBarFillW > 0, "settings OTA progress fill width must be positive");
static_assert(kSettingsOtaBarFillH > 0, "settings OTA progress fill height must be positive");
static_assert(kSettingsOtaProgressMax > 0, "settings OTA progress maximum must be positive");
}

void build_settings_ota_panel(lv_obj_t *screen, int panel_x, int panel_width)
{
    s_settings_ota_status_label = make_centered_label(screen,
                                                      panel_x,
                                                      176,
                                                      panel_width,
                                                      22,
                                                      "",
                                                      "settings ota status label create failed");
    s_settings_ota_bar_frame = lv_obj_create(screen);
    if (s_settings_ota_bar_frame) {
        lv_obj_clear_flag(s_settings_ota_bar_frame, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(s_settings_ota_bar_frame, kSettingsOtaBarFrameX, kSettingsOtaBarFrameY);
        lv_obj_set_size(s_settings_ota_bar_frame, kSettingsOtaBarFrameW, kSettingsOtaBarFrameH);
        lv_obj_set_style_bg_color(s_settings_ota_bar_frame, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_settings_ota_bar_frame, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_settings_ota_bar_frame, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_settings_ota_bar_frame, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(s_settings_ota_bar_frame, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_settings_ota_bar_frame, 0, LV_PART_MAIN);
        lv_obj_add_flag(s_settings_ota_bar_frame, LV_OBJ_FLAG_HIDDEN);
    } else {
        ESP_LOGW(TAG, "settings ota bar frame create failed");
    }
    s_settings_ota_bar_fill = make_black_bar(screen,
                                             kSettingsOtaBarFrameX + kSettingsOtaBarInset,
                                             kSettingsOtaBarFrameY + kSettingsOtaBarInset,
                                             1,
                                             kSettingsOtaBarFillH);
    if (s_settings_ota_bar_fill) {
        lv_obj_set_style_radius(s_settings_ota_bar_fill, 2, LV_PART_MAIN);
        lv_obj_add_flag(s_settings_ota_bar_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        ESP_LOGW(TAG, "settings ota bar fill create failed");
    }
    s_settings_ota_hint_label = make_centered_label(screen,
                                                    panel_x,
                                                    218,
                                                    panel_width,
                                                    20,
                                                    "",
                                                    "settings ota hint label create failed");
}

bool update_settings_ota_panel(bool visible, const OtaRuntimeSnapshot &ota)
{
    if (!s_settings_ota_status_label) {
        return false;
    }

    bool changed = false;
    char ota_line[kSettingsOtaLineTextSize] = "";
    char ota_hint[kSettingsOtaHintTextSize] = "";
    bool progress_visible = false;
    int progress = ota.progress;
    if (visible) {
        if (ota.state == kOtaUpdating && progress >= 0) {
            progress_visible = true;
            if (ota.speed_kbps > 0) {
                ui_text::format_or_fallback(ota_line,
                                            sizeof(ota_line),
                                            kSettingsOtaLinePlaceholder,
                                            kSettingsOtaUpdatingWithSpeedFormat,
                                            progress,
                                            ota.speed_kbps);
            } else {
                ui_text::format_or_fallback(ota_line,
                                            sizeof(ota_line),
                                            kSettingsOtaLinePlaceholder,
                                            kSettingsOtaUpdatingFormat,
                                            progress);
            }
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintDownloading);
        } else if (ota.state == kOtaAvailable) {
            ui_text::copy(ota_line, sizeof(ota_line), ota.status);
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintInstall);
        } else if (ota.state == kOtaChecking) {
            ui_text::copy(ota_line, sizeof(ota_line), ota.status);
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintChecking);
        } else if (ota.state == kOtaSucceeded) {
            progress_visible = true;
            progress = kSettingsOtaProgressMax;
            ui_text::copy(ota_line, sizeof(ota_line), ota.status);
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintRebooting);
        } else if (ota.state == kOtaFailed || ota.state == kOtaNoUpdate) {
            ui_text::copy(ota_line, sizeof(ota_line), ota.status);
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintRetry);
        } else {
            ui_text::format_or_fallback(ota_line,
                                        sizeof(ota_line),
                                        kSettingsOtaLinePlaceholder,
                                        kSettingsOtaCurrentVersionFormat,
                                        APP_VERSION);
            ui_text::copy(ota_hint, sizeof(ota_hint), kSettingsOtaHintCheck);
        }
    }
    changed |= set_label_text_if_changed(s_settings_ota_status_label, ota_line);
    if (s_settings_ota_hint_label) {
        changed |= set_label_text_if_changed(s_settings_ota_hint_label, ota_hint);
    }
    bool show_progress = visible && progress_visible;
    changed |= set_obj_visible(s_settings_ota_bar_frame, show_progress);
    if (s_settings_ota_bar_fill) {
        changed |= set_settings_ota_fill_width(settings_ota_progress_fill_width(progress));
        changed |= set_obj_visible(s_settings_ota_bar_fill, show_progress);
    }
    return changed;
}

void clear_settings_ota_panel_object_refs()
{
    s_settings_ota_status_label = nullptr;
    s_settings_ota_hint_label = nullptr;
    s_settings_ota_bar_frame = nullptr;
    s_settings_ota_bar_fill = nullptr;
}
