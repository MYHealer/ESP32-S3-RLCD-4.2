// 构建并刷新 System Info 与网络检测两个独立辅助页面。
#include "ui_aux_pages.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "battery_runtime_state.h"
#include "network_credentials_state.h"
#include "network_diagnostics_catalog.h"
#include "network_diagnostics_state.h"
#include "ntp_services.h"
#include "ota_services.h"
#include "ui_fonts.h"
#include "ui_page_state.h"
#include "ui_text_format.h"
#include "ui_time_format.h"
#include "ui_widgets.h"
#include "weather_state.h"

#include <esp_attr.h>
#include <esp_log.h>

namespace {
constexpr int kNetworkDiagGridFirstLine = kNetworkDiagIpLocationLine;
constexpr int kNetworkDiagWideLine = kNetworkDiagOtaLine;
constexpr int kNetworkDiagGridColumns = 2;
constexpr int kNetworkDiagWideX = 30;
constexpr int kNetworkDiagWideW = 340;
constexpr int kNetworkDiagLocalIpY = 88;
constexpr int kNetworkDiagPublicIpY = 112;
constexpr int kNetworkDiagGridStartY = 142;
constexpr int kNetworkDiagGridRowGap = 28;
constexpr int kNetworkDiagGridColGap = 174;
constexpr int kNetworkDiagGridW = 160;
constexpr size_t kNetworkDiagSummaryTextSize = 64;
constexpr int kAuxPageTitleX = 24;
constexpr int kAuxPageTitleY = 18;
constexpr int kAuxPageTitleW = 352;
constexpr int kInfoPageTitleH = 26;
constexpr int kNetworkDiagTitleH = 28;
constexpr int kAuxPageLineX = 24;
constexpr int kAuxPageLineW = 352;
constexpr int kInfoPageTopLineY = 50;
constexpr int kInfoPageTopLineH = 3;
constexpr int kInfoPageBottomLineY = 238;
constexpr int kInfoPageBottomLineH = 3;
constexpr int kInfoReturnHintX = 24;
constexpr int kInfoReturnHintY = 252;
constexpr int kInfoReturnHintW = 352;
constexpr int kInfoReturnHintH = 22;
constexpr int kNetworkDiagTopLineY = 52;
constexpr int kNetworkDiagTopLineH = 3;
constexpr int kNetworkDiagSummaryX = 24;
constexpr int kNetworkDiagSummaryY = 62;
constexpr int kNetworkDiagSummaryW = 352;
constexpr int kNetworkDiagSummaryH = 22;
constexpr int kNetworkDiagLineH = 22;
constexpr int kNetworkDiagBottomLineY = 266;
constexpr int kNetworkDiagBottomLineH = 2;
constexpr int kNetworkDiagHintX = 24;
constexpr int kNetworkDiagHintY = 272;
constexpr int kNetworkDiagHintW = 352;
constexpr int kNetworkDiagHintH = 20;
EXT_RAM_BSS_ATTR lv_obj_t *s_network_diag_labels[kNetworkDiagLineCount];
lv_obj_t *s_network_diag_summary_label;
lv_obj_t *s_network_diag_hint_label;
EXT_RAM_BSS_ATTR NetworkDiagnosticsSnapshot s_network_diag_render_snapshot;
constexpr const char *kNetworkDiagTitle = "网络检测";
constexpr const char *kNetworkDiagSummaryReady = "准备检测...";
constexpr const char *kNetworkDiagSummaryRunning = "检测中...";
constexpr const char *kNetworkDiagSummaryDone = "检测完成";
constexpr const char *kNetworkDiagSummaryIdle = "等待开始";
constexpr const char *kNetworkDiagLinePlaceholder = "--";
constexpr const char *kNetworkDiagHintIdle = "Hold KEY to return";
constexpr const char *kNetworkDiagHintRunning = "Checking... Hold KEY to return";
constexpr size_t kInfoTimeTextSize = 32;
constexpr size_t kInfoLineTextSize = 96;
constexpr const char *kInfoLastNtpFormat = "Last NTP: %s";
constexpr const char *kInfoWifiFormat = "WiFi: %s";
constexpr const char *kInfoLastWeatherFormat = "Last Weather: %s";
constexpr const char *kInfoBatteryFullFormat = "Battery: %d%%  %.2fV \\ %s";
constexpr const char *kInfoBatteryPercentOnlyFormat = "Battery: %d%%  -- \\ %s";
constexpr const char *kInfoBatteryPlaceholder = "Battery: --  -- \\ --";
constexpr const char *kInfoVersionFormat = "Version: %s / %s";
constexpr const char *kInfoSourceFormat = "Source: %s";
constexpr const char *kProjectSourceUrl = "github.com/wickenzh/ESP32-S3-RLCD-4.2";
constexpr const char *kInfoLinePlaceholder = "--";
constexpr const char *kInfoReturnHintText = "Hold KEY to return";
constexpr int kInfoTextX = 30;
constexpr int kInfoTextW = 340;
constexpr int kInfoSourceTextX = 0;
constexpr int kInfoSourceTextW = 400;
constexpr int kInfoLabelY[] = {70, 104, 138, 172, 206, 276};
constexpr size_t kInfoLabelCount = array_count(kInfoLabelY);
EXT_RAM_BSS_ATTR lv_obj_t *s_info_labels[kInfoLabelCount];
constexpr size_t kInfoNtpLabelIndex = 0;
constexpr size_t kInfoWifiLabelIndex = 1;
constexpr size_t kInfoWeatherLabelIndex = 2;
constexpr size_t kInfoBatteryLabelIndex = 3;
constexpr size_t kInfoVersionLabelIndex = 4;
constexpr size_t kInfoSourceLabelIndex = kInfoLabelCount - 1;

#define NETWORK_DIAG_LINE_LABEL_CREATE_FAILED_FORMAT "network diag line %d label create failed"

static_assert(kNetworkDiagLocalIpLine < kNetworkDiagPublicIpLine,
              "network diagnostics local IP line must precede public IP line");
static_assert(kNetworkDiagPublicIpLine < kNetworkDiagGridFirstLine,
              "network diagnostics public IP line must precede grid lines");
static_assert(kNetworkDiagGridFirstLine <= kNetworkDiagWideLine,
              "network diagnostics grid first line must not follow wide line");
static_assert(kNetworkDiagWideLine < kNetworkDiagLineCount,
              "network diagnostics wide line must fit line count");
static_assert(kNetworkDiagGridColumns > 0, "network diagnostics grid must have columns");
static_assert(kNetworkDiagWideW > 0 && kNetworkDiagGridW > 0,
              "network diagnostics line widths must be positive");
static_assert(kNetworkDiagWideW >= kNetworkDiagGridW,
              "network diagnostics wide line must fit grid line width");
static_assert(kAuxPageTitleW > 0 && kAuxPageLineW > 0, "auxiliary page frame widths must be positive");
static_assert(kInfoPageTitleH > 0 && kNetworkDiagTitleH > 0,
              "auxiliary page title heights must be positive");
static_assert(kInfoReturnHintW > 0 && kInfoReturnHintH > 0,
              "System Info return hint size must be positive");
static_assert(kNetworkDiagSummaryW > 0 && kNetworkDiagSummaryH > 0,
              "network diagnostics summary size must be positive");
static_assert(kNetworkDiagLineH > 0, "network diagnostics line height must be positive");
static_assert(kNetworkDiagHintW > 0 && kNetworkDiagHintH > 0,
              "network diagnostics hint size must be positive");
static_assert(kNetworkDiagSummaryTextSize > 1,
              "network diagnostics summary buffer must fit text and NUL");
static_assert(sizeof(NetworkDiagnosticsSnapshot) ==
                  sizeof(NetworkDiagState) +
                      kNetworkDiagLineCount * kNetworkDiagLineLen,
              "network diagnostics render snapshot must contain only state and lines");
static_assert(kInfoLabelCount == array_count(s_info_labels),
              "System Info labels and row coordinates must stay in sync");
static_assert(kInfoVersionLabelIndex < kInfoSourceLabelIndex,
              "System Info version label must precede source label");
static_assert(kInfoSourceLabelIndex < kInfoLabelCount,
              "System Info source label index must fit label count");

struct NetworkDiagLineLayout {
    int x;
    int y;
    int w;
};

NetworkDiagLineLayout network_diag_line_layout(int index)
{
    NetworkDiagLineLayout layout = {kNetworkDiagWideX, kNetworkDiagLocalIpY, kNetworkDiagWideW};
    if (index == kNetworkDiagLocalIpLine) {
        layout.y = kNetworkDiagLocalIpY;
    } else if (index == kNetworkDiagPublicIpLine) {
        layout.y = kNetworkDiagPublicIpY;
    } else {
        int grid = index - kNetworkDiagGridFirstLine;
        int row = grid / kNetworkDiagGridColumns;
        int col = grid % kNetworkDiagGridColumns;
        layout.x = kNetworkDiagWideX + col * kNetworkDiagGridColGap;
        layout.y = kNetworkDiagGridStartY + row * kNetworkDiagGridRowGap;
        layout.w = kNetworkDiagGridW;
        if (index == kNetworkDiagWideLine) {
            layout.x = kNetworkDiagWideX;
            layout.w = kNetworkDiagWideW;
        }
    }
    return layout;
}

bool set_info_time_label(size_t index, const char *format, time_t value)
{
    char time_text[kInfoTimeTextSize] = {};
    char line[kInfoLineTextSize] = {};
    format_time_or_dash(value, time_text, sizeof(time_text));
    ui_text::format_or_fallback(line, sizeof(line), kInfoLinePlaceholder, format, time_text);
    return set_label_text_if_changed(s_info_labels[index], line);
}

bool set_info_string_label(size_t index, const char *format, const char *value)
{
    char line[kInfoLineTextSize] = {};
    ui_text::format_or_fallback(line, sizeof(line), kInfoLinePlaceholder, format, value ? value : "");
    return set_label_text_if_changed(s_info_labels[index], line);
}

bool set_info_battery_label()
{
    BatteryRuntimeSnapshot battery;
    if (!battery_runtime_snapshot_load(&battery)) {
        return false;
    }
    char line[kInfoLineTextSize] = {};
    char charge_time[kInfoTimeTextSize] = {};
    format_time_or_dash(battery.last_full_charge_time, charge_time, sizeof(charge_time));
    if (battery.percent >= 0 && battery.voltage >= 0.0f) {
        ui_text::format_or_fallback(line, sizeof(line), kInfoBatteryPlaceholder, kInfoBatteryFullFormat,
                                    battery.percent, battery.voltage, charge_time);
    } else if (battery.percent >= 0) {
        ui_text::format_or_fallback(line, sizeof(line), kInfoBatteryPlaceholder, kInfoBatteryPercentOnlyFormat,
                                    battery.percent, charge_time);
    } else {
        ui_text::copy(line, sizeof(line), kInfoBatteryPlaceholder);
    }
    return set_label_text_if_changed(s_info_labels[kInfoBatteryLabelIndex], line);
}

bool set_info_version_label()
{
    char line[kInfoLineTextSize] = {};
    ui_text::format_or_fallback(line, sizeof(line), kInfoLinePlaceholder, kInfoVersionFormat, APP_VERSION, APP_BUILD_DATE);
    return set_label_text_if_changed(s_info_labels[kInfoVersionLabelIndex], line);
}
} // namespace

void build_boot_info_page()
{
    if (auxiliary_page_root(AuxiliaryPage::kSystemInfo)) {
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_auxiliary_page_root(AuxiliaryPage::kSystemInfo, screen);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);

    make_centered_label_with_font(screen, kAuxPageTitleX, kAuxPageTitleY, kAuxPageTitleW, kInfoPageTitleH,
                                  "SYSTEM INFO", &lv_font_montserrat_16, "system info title create failed");
    make_black_bar(screen, kAuxPageLineX, kInfoPageTopLineY, kAuxPageLineW, kInfoPageTopLineH);
    for (size_t i = 0; i < kInfoLabelCount; ++i) {
        const bool source_line = i == kInfoSourceLabelIndex;
        s_info_labels[i] = make_label_with_font(screen,
                                                source_line ? kInfoSourceTextX : kInfoTextX,
                                                kInfoLabelY[i],
                                                source_line ? kInfoSourceTextW : kInfoTextW,
                                                source_line ? 18 : 24,
                                                kInfoLinePlaceholder,
                                                source_line ? &lv_font_montserrat_12 : &lv_font_montserrat_14);
        if (source_line) {
            (void)center_align_label(s_info_labels[i]);
        }
    }
    make_black_bar(screen, kAuxPageLineX, kInfoPageBottomLineY, kAuxPageLineW, kInfoPageBottomLineH);
    make_centered_label_with_font(screen, kInfoReturnHintX, kInfoReturnHintY,
                                  kInfoReturnHintW, kInfoReturnHintH, kInfoReturnHintText,
                                  &lv_font_montserrat_14, "system info return label create failed");
}

bool update_boot_info_page()
{
    bool changed = false;
    char wifi_ssid[kNetworkWifiSsidLen] = {};
    WeatherCacheStatusSnapshot weather_cache = {};
    (void)network_wifi_ssid_snapshot(wifi_ssid, sizeof(wifi_ssid));
    const bool weather_cache_loaded =
        weather_cache_status_snapshot_load(&weather_cache);
    changed |= set_info_time_label(kInfoNtpLabelIndex,
                                   kInfoLastNtpFormat,
                                   get_last_ntp_sync_time());
    changed |= set_info_string_label(kInfoWifiLabelIndex,
                                     kInfoWifiFormat,
                                     wifi_ssid[0] ? wifi_ssid : "--");
    if (weather_cache_loaded) {
        changed |= set_info_time_label(kInfoWeatherLabelIndex,
                                       kInfoLastWeatherFormat,
                                       weather_cache.last_sync_time);
    }
    changed |= set_info_battery_label();
    changed |= set_info_version_label();
    changed |= set_info_string_label(kInfoSourceLabelIndex,
                                     kInfoSourceFormat,
                                     kProjectSourceUrl);
    ota_reset_status_if_idle();
    return changed;
}

void build_network_diag_page()
{
    if (auxiliary_page_root(AuxiliaryPage::kNetworkDiagnostics)) {
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_auxiliary_page_root(AuxiliaryPage::kNetworkDiagnostics, screen);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    make_centered_label(screen, kAuxPageTitleX, kAuxPageTitleY, kAuxPageTitleW, kNetworkDiagTitleH,
                        kNetworkDiagTitle, "network diag title create failed");
    make_black_bar(screen, kAuxPageLineX, kNetworkDiagTopLineY, kAuxPageLineW, kNetworkDiagTopLineH);
    s_network_diag_summary_label = make_centered_label(screen, kNetworkDiagSummaryX, kNetworkDiagSummaryY,
                                                       kNetworkDiagSummaryW, kNetworkDiagSummaryH,
                                                       kNetworkDiagSummaryReady,
                                                       "network diag summary label create failed");
    for (int i = 0; i < kNetworkDiagLineCount; ++i) {
        NetworkDiagLineLayout layout = network_diag_line_layout(i);
        s_network_diag_labels[i] = make_label(screen, layout.x, layout.y, layout.w, kNetworkDiagLineH,
                                              kNetworkDiagLinePlaceholder);
        if (s_network_diag_labels[i]) {
            lv_label_set_long_mode(s_network_diag_labels[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(s_network_diag_labels[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        } else {
            ESP_LOGW(TAG, NETWORK_DIAG_LINE_LABEL_CREATE_FAILED_FORMAT, i);
        }
    }
    make_black_bar(screen, kAuxPageLineX, kNetworkDiagBottomLineY, kAuxPageLineW, kNetworkDiagBottomLineH);
    s_network_diag_hint_label = make_centered_label(screen, kNetworkDiagHintX, kNetworkDiagHintY,
                                                    kNetworkDiagHintW, kNetworkDiagHintH, kNetworkDiagHintIdle,
                                                    "network diag hint label create failed");
}

bool update_network_diag_page()
{
    bool changed = false;
    NetworkDiagnosticsSnapshot &snapshot = s_network_diag_render_snapshot;
    if (!network_diag_snapshot_load(&snapshot)) {
        return false;
    }
    char summary[kNetworkDiagSummaryTextSize] = {};
    if (snapshot.state == kNetworkDiagRunning) {
        ui_text::copy(summary, sizeof(summary), kNetworkDiagSummaryRunning);
    } else if (snapshot.state == kNetworkDiagDone) {
        ui_text::copy(summary, sizeof(summary), kNetworkDiagSummaryDone);
    } else {
        ui_text::copy(summary, sizeof(summary), kNetworkDiagSummaryIdle);
    }
    changed |= set_label_text_if_changed(s_network_diag_summary_label, summary);
    for (int i = 0; i < kNetworkDiagLineCount; ++i) {
        changed |= set_label_text_if_changed(s_network_diag_labels[i],
                                             snapshot.lines[i][0] ? snapshot.lines[i] :
                                                                    kNetworkDiagLinePlaceholder);
    }
    changed |= set_label_text_if_changed(s_network_diag_hint_label,
                                         snapshot.state == kNetworkDiagRunning ? kNetworkDiagHintRunning :
                                                                                kNetworkDiagHintIdle);
    return changed;
}

void clear_aux_page_object_refs()
{
    for (lv_obj_t *&label : s_info_labels) {
        label = nullptr;
    }
    for (lv_obj_t *&label : s_network_diag_labels) {
        label = nullptr;
    }
    s_network_diag_summary_label = nullptr;
    s_network_diag_hint_label = nullptr;
}
