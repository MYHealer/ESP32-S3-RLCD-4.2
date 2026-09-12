// 管理页面根对象、可见性、工作页顺序和低电量显示状态。
#include "ui_page_state.h"
#include "ui_aggregate_clock.h"

#include "active_work_page_state_internal.h"
#include "app_display_config.h"
#include "app_metadata.h"
#include "battery_runtime_state.h"
#include "ui_clock.h"
#include "ui_clock_header_objects.h"
#include "ui_clock_surface_objects.h"
#include "ui_flip_clock.h"
#include "ui_progress.h"
#include "ui_setup_status.h"
#include "ui_status_refresh_policy.h"
#include "ui_widgets.h"
#include "ui_work_page_catalog.h"
#include "ui_work_pages.h"
#include "ui_xiaozhi.h"
#include "weather_state.h"
#include "wifi_portal_state.h"
#include "work_page_ids.h"

#include <esp_attr.h>
#include <esp_log.h>

#include <stddef.h>

namespace {
#define PAGE_ROOT_CREATE_FAILED_LOG "page root create failed"
#define LOWER_PANEL_OBJECT_LIST_FULL_LOG "lower panel object list full"
constexpr int kFallbackWorkPage = kWorkPageWeatherClock;
constexpr size_t kAuxPageRootCount = static_cast<size_t>(AuxiliaryPage::kCount);
constexpr int kPageRootX = 0;
constexpr int kPageRootY = 0;
constexpr int kPageRootW = kDisplayWidth;
constexpr int kPageRootH = kDisplayHeight;
constexpr size_t kLowerPanelObjectCapacity = 13;
EXT_RAM_BSS_ATTR lv_obj_t *s_work_page_roots[kWorkPageCount];
EXT_RAM_BSS_ATTR lv_obj_t *s_auxiliary_page_roots[kAuxPageRootCount];
EXT_RAM_BSS_ATTR lv_obj_t *s_lower_panel_objects[kLowerPanelObjectCapacity];

size_t auxiliary_page_index(AuxiliaryPage page)
{
    return static_cast<size_t>(page);
}

lv_obj_t *work_page_root_or_fallback(lv_obj_t *root)
{
    return root ? root : work_page_root(kFallbackWorkPage);
}

lv_obj_t *build_work_page_root(int page)
{
    switch (page) {
    case kWorkPageWeatherClock:
        build_clock_ui();
        break;
    case kWorkPageHistory:
        build_history_page();
        break;
    case kWorkPageGallery:
        build_gallery_page();
        break;
    case kWorkPageCalendar:
        build_calendar_page();
        break;
    case kWorkPageWeatherBoard:
        build_weather_board_page();
        break;
    case kWorkPageFlipClock:
        build_flip_clock_page();
        break;
    case kWorkPageXiaozhiAI:
        build_xiaozhi_page();
        break;
    case kWorkPageAggregateClock:
        build_aggregate_clock_page();
        break;
    default:
        return work_page_root(kFallbackWorkPage);
    }
    return work_page_root_or_fallback(work_page_root(page));
}

static_assert(kFallbackWorkPage == kWorkPageWeatherClock, "special-mode fallback page must remain weather clock");
static_assert(kAuxPageRootCount == 3, "auxiliary roots are info, network diagnostics and settings");
static_assert(kPageRootW > 0 && kPageRootH > 0, "page root size must be positive");
static_assert(kLowerPanelObjectCapacity > 0, "lower panel object storage must not be empty");
} // namespace

lv_obj_t *work_page_root(int page)
{
    if (!is_valid_work_page_id(page)) {
        return nullptr;
    }
    return s_work_page_roots[page];
}

void set_work_page_root(int page, lv_obj_t *root)
{
    if (!is_valid_work_page_id(page)) {
        return;
    }
    s_work_page_roots[page] = root;
}

void clear_work_page_root_refs()
{
    for (lv_obj_t *&root : s_work_page_roots) {
        root = nullptr;
    }
}

lv_obj_t *auxiliary_page_root(AuxiliaryPage page)
{
    size_t index = auxiliary_page_index(page);
    return index < kAuxPageRootCount ? s_auxiliary_page_roots[index] : nullptr;
}

void set_auxiliary_page_root(AuxiliaryPage page, lv_obj_t *root)
{
    size_t index = auxiliary_page_index(page);
    if (index < kAuxPageRootCount) {
        s_auxiliary_page_roots[index] = root;
    }
}

void clear_auxiliary_page_root_refs()
{
    for (lv_obj_t *&root : s_auxiliary_page_roots) {
        root = nullptr;
    }
}

static void configure_page_root(lv_obj_t *root)
{
    if (!root) {
        return;
    }
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(root, kPageRootX, kPageRootY);
    lv_obj_set_size(root, kPageRootW, kPageRootH);
    lv_obj_set_style_bg_color(root, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
}

lv_obj_t *create_page_root()
{
    lv_obj_t *root = lv_obj_create(lv_scr_act());
    if (!root) {
        ESP_LOGW(TAG, "%s", PAGE_ROOT_CREATE_FAILED_LOG);
        return nullptr;
    }
    configure_page_root(root);
    return root;
}

void set_page_visible(lv_obj_t *page, bool visible)
{
    if (!page) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(page);
    } else {
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_page(lv_obj_t *page)
{
    for (lv_obj_t *root : s_work_page_roots) {
        set_page_visible(root, page == root);
    }
    for (lv_obj_t *root : s_auxiliary_page_roots) {
        set_page_visible(root, page == root);
    }
    if (page) {
        lv_obj_invalidate(page);
    }
}

lv_obj_t *active_work_page_root()
{
    if (battery_low_mode_load() || setup_portal_active_load()) {
        active_work_page_store(kFallbackWorkPage);
        return build_work_page_root(kFallbackWorkPage);
    }
    ensure_active_work_page_enabled();
    return build_work_page_root(active_work_page_load());
}

void show_active_work_page()
{
    show_page(active_work_page_root());
}

void remember_lower_panel_object(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }
    for (lv_obj_t *&slot : s_lower_panel_objects) {
        if (!slot) {
            slot = obj;
            return;
        }
    }
    ESP_LOGW(TAG, "%s", LOWER_PANEL_OBJECT_LIST_FULL_LOG);
}

bool set_obj_visible(lv_obj_t *obj, bool visible)
{
    if (!obj) {
        return false;
    }
    bool already_visible = !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (already_visible == visible) {
        return false;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

void set_lower_panel_visible(bool visible)
{
    for (lv_obj_t *obj : s_lower_panel_objects) {
        set_obj_visible(obj, visible);
    }
}

void clear_lower_panel_object_refs()
{
    for (lv_obj_t *&obj : s_lower_panel_objects) {
        obj = nullptr;
    }
}

void apply_clock_mode_visibility(bool setup_active, bool low_battery_mode)
{
    const ClockHeaderObjectRefs &header = clock_header_object_refs();
    const ClockSurfaceObjectRefs &surface = clock_surface_object_refs();
    const bool low = low_battery_mode;
    set_obj_visible(surface.second_canvas, !low && !setup_active);
    set_work_page_day_progress_visible(kWorkPageWeatherClock, !low);
    set_obj_visible(surface.second_progress_canvas, !low && !setup_active);
    set_obj_visible(surface.low_battery_icon_canvas, low);
    set_lower_panel_visible(!setup_active && !low);
    set_setup_status_panel_visible(setup_active && !low);
    set_obj_visible(surface.panel_separator_a, !setup_active || low);
    set_obj_visible(surface.panel_separator_b, !setup_active || low);
    if (low || setup_active) {
        set_obj_visible(header.alert_pill, false);
        set_obj_visible(header.chime_status_icon_canvas, false);
        set_obj_visible(header.wifi_status_icon_canvas, false);
        set_obj_visible(header.alarm_status_icon_canvas, false);
    }
}

void update_alert_pill(bool show,
                       int alert_index,
                       const UiStatusRefreshSnapshot &status,
                       bool low_battery_mode,
                       bool setup_active)
{
    const ClockHeaderObjectRefs &header = clock_header_object_refs();
    char title[kWeatherAlertTitleLen] = {};
    bool visible = show &&
                   !low_battery_mode &&
                   !setup_active &&
                   get_weather_alert_title_snapshot(alert_index,
                                                    title,
                                                    sizeof(title));
    set_obj_visible(header.alert_pill, visible);
    update_top_status_icons(visible,
                            status,
                            low_battery_mode,
                            setup_active);
    if (visible) {
        set_label_text_if_changed(header.alert_label, title);
    }
}

bool update_top_status_icons(bool alert_visible,
                             const UiStatusRefreshSnapshot &status,
                             bool low_battery_mode,
                             bool setup_active)
{
    const ClockHeaderObjectRefs &header = clock_header_object_refs();
    const bool allow = !alert_visible && !low_battery_mode && !setup_active;
    bool changed = false;
    changed |= set_obj_visible(header.chime_status_icon_canvas,
                               allow && status.chime_enabled);
    changed |= set_obj_visible(header.wifi_status_icon_canvas,
                               allow && status.wifi_radio_on);
    changed |= set_obj_visible(header.alarm_status_icon_canvas,
                               allow && status.alarm_enabled);
    return changed;
}
