// 构建并刷新设置页及其菜单交互。
#include "ui_settings_page.h"
#include "ui_settings_visual_style.h"

#include "alarm_services.h"
#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_tick_time.h"
#include "chime_runtime_state.h"

#include "ota_runtime_state.h"
#include "ota_services.h"
#include "ui_page_state.h"
#include "ui_settings_content.h"
#include "ui_settings_feedback.h"
#include "ui_settings_layout.h"
#include "ui_settings_navigation.h"
#include "ui_settings_ota_panel.h"
#include "ui_widgets.h"
#include "ui_work_page_catalog.h"
#include "work_page_ids.h"
#include "xiaozhi_auto_return_state.h"

#include <esp_attr.h>
#include <esp_log.h>

#include <string.h>

namespace {
namespace settings_layout = ui_settings_layout;

EXT_RAM_BSS_ATTR lv_obj_t *s_settings_labels[kSettingsLabelCount];
EXT_RAM_BSS_ATTR lv_obj_t *s_settings_switch_dots[kSettingsSecondaryMaxCount];
lv_obj_t *s_settings_feedback_label;

struct SettingsRenderWorkspace {
    char secondary_items[kSettingsSecondaryMaxCount][kSettingsSecondaryTextSize];
};

struct SettingsRenderCache {
    uint32_t magic;
    lv_obj_t *settings_root;
    int primary;
    int selected;
    int ota_state;
    int ota_progress;
    int ota_speed;
    int page_order_selection;
    bool focus_secondary;
    bool page_order_mode;
    bool page_toggle_mode;
};

// update_settings_page() is serialized by the UI task. Keep its complete text
// staging area out of that task's call stack while preserving rebuild-before-use.
EXT_RAM_BSS_ATTR SettingsRenderWorkspace s_settings_render_workspace;
// The same UI task owns this low-frequency comparison cache. Keep it outside
// internal RAM and restore the non-zero sentinels explicitly before first use.
EXT_RAM_BSS_ATTR SettingsRenderCache s_settings_render_cache;
constexpr uint32_t kSettingsRenderCacheMagic = 0x53455443U; // "SETC"

static_assert(sizeof(SettingsRenderWorkspace) ==
                  kSettingsSecondaryMaxCount * kSettingsSecondaryTextSize,
              "settings render workspace must contain only secondary text storage");
static_assert(sizeof(SettingsRenderCache) <= 48,
              "settings render cache must remain a compact UI-only state");
static_assert(kWorkPageCount <= 8,
              "settings switch snapshot stores the work-page mask in one byte");
static_assert(sizeof(SettingsSecondaryStateSnapshot) <= 12,
              "settings secondary snapshot must remain lightweight");

SettingsSecondaryStateSnapshot settings_secondary_state_snapshot(
    int primary,
    const SettingsNavigationSnapshot &navigation)
{
    SettingsSecondaryStateSnapshot snapshot = {};
    if (primary == kSettingsPrimarySound) {
        const ChimeRuntimeSnapshot chime = chime_runtime_snapshot_load();
        snapshot.hourly_chime_enabled = chime.hourly_enabled;
        snapshot.all_day_chime_enabled = chime.all_day;
        snapshot.volume_percent = chime.volume_percent;
        snapshot.sound_index = chime.sound_index;
    } else if (primary == kSettingsPrimaryDisplay) {
        if (navigation.page_toggle_mode || navigation.page_order_mode) {
            snapshot.work_page_enabled_mask = work_page_enabled_mask_load();
        } else if (!navigation.page_order_mode) {
            AlarmSnapshot alarm = {};
            alarm_get_snapshot(&alarm);
            snapshot.alarm_enabled = alarm.enabled;
            snapshot.alarm_hour = alarm.hour;
            snapshot.alarm_minute = alarm.minute;
            snapshot.xiaozhi_auto_return_enabled =
                xiaozhi_auto_return_enabled_load();
        }
    }
    return snapshot;
}

bool work_page_enabled_in_switch_snapshot(
    const SettingsSecondaryStateSnapshot &snapshot,
    int page)
{
    if (!is_valid_work_page_id(page)) {
        return false;
    }
    return (snapshot.work_page_enabled_mask &
            static_cast<uint8_t>(1U << page)) != 0;
}

int collect_visible_work_page_order(int *indices,
                                    uint8_t *pages,
                                    size_t capacity,
                                    uint8_t enabled_mask)
{
    if (!indices || !pages || capacity == 0) {
        return 0;
    }
    uint8_t order[kWorkPageCount] = {};
    if (!work_page_order_copy(order, sizeof(order))) {
        return 0;
    }
    int count = 0;
    for (int order_index = 0; order_index < kWorkPageCount && (size_t)count < capacity; ++order_index) {
        if ((enabled_mask & static_cast<uint8_t>(1U << order[order_index])) != 0) {
            indices[count] = order_index;
            pages[count] = order[order_index];
            ++count;
        }
    }
    return count;
}

#define SETTINGS_PRIMARY_LABEL_CREATE_FAILED_FORMAT "settings primary label create failed index=%d"
#define SETTINGS_SECONDARY_LABEL_CREATE_FAILED_FORMAT "settings secondary label create failed index=%d"
#define SETTINGS_SWITCH_DOT_CREATE_FAILED_FORMAT "settings switch dot create failed index=%d"

constexpr const char *kSettingsPrimaryItems[kSettingsPrimaryCount] = {"网络", "声音", "显示", "系统"};
constexpr const char *kSettingsPageOrderEntryFormat = "%d %s";
#define SETTINGS_SWITCH_SLOT_INDEX_OUT_OF_RANGE_FORMAT "settings switch slot index out of range: %d"
constexpr const char *kSettingsLabelPlaceholder = "--";

enum SettingsMenuColumn {
    kSettingsMenuPrimaryColumn,
    kSettingsMenuSecondaryColumn,
};

int settings_long_item_y(int primary)
{
    return primary == kSettingsPrimarySystem
               ? settings_layout::kSettingsSystemLongItemY
               : settings_layout::kSettingsDisplayLongItemY;
}

void reset_settings_render_cache()
{
    SettingsRenderCache &cache = s_settings_render_cache;
    cache = {};
    cache.magic = kSettingsRenderCacheMagic;
    cache.primary = -1;
    cache.selected = -1;
    cache.ota_state = -1;
    cache.ota_progress = -2;
    cache.ota_speed = -2;
    cache.page_order_selection = -1;
}

SettingsRenderCache &settings_render_cache()
{
    if (s_settings_render_cache.magic != kSettingsRenderCacheMagic) {
        reset_settings_render_cache();
    }
    return s_settings_render_cache;
}

static_assert(settings_layout::kSettingsListRowCount == kSettingsSecondaryMaxCount,
              "settings list rows must match secondary slot count");
static_assert(settings_layout::kSettingsGridCapacity >= kWorkPageCount,
              "settings grid capacity must cover all work pages");
static_assert(settings_layout::kSettingsGridCapacity >= kSystemSettingsGridItemCount,
              "settings grid capacity must cover system grid items");
static_assert(settings_layout::kSettingsGridCapacity >= kDisplaySettingsGridItemCount,
              "settings grid capacity must cover display grid items");
static_assert(array_count(kSettingsPrimaryItems) == kSettingsPrimaryCount,
              "settings primary item table must match primary count");
static_assert(array_count(s_settings_labels) == kSettingsLabelCount,
              "settings label storage must match configured label count");
static_assert(array_count(s_settings_switch_dots) == kSettingsSecondaryMaxCount,
              "settings switch dot storage must match secondary slot count");
static_assert(cstr_array_nonempty(kSettingsPrimaryItems), "settings primary menu texts must be non-empty");
static_assert(settings_layout::kSettingsGridColumns > 0,
              "settings grid must have columns");
static_assert(settings_layout::kSettingsGridColW > 0 &&
                  settings_layout::kSettingsSecondaryH > 0,
              "settings grid item size must be positive");
static_assert(settings_layout::kSettingsSystemLongItemY >= 0 &&
                  settings_layout::kSettingsDisplayLongItemY >= 0,
              "settings long item y positions must be non-negative");
static_assert(settings_layout::kSettingsListRowCount >= kSettingsPrimaryCount,
              "settings list rows must fit primary menu items");

void hide_settings_switch_slot(int index)
{
    if (!settings_secondary_index_valid(index)) {
        ESP_LOGW(TAG, SETTINGS_SWITCH_SLOT_INDEX_OUT_OF_RANGE_FORMAT, index);
        return;
    }
    if (s_settings_switch_dots[index]) {
        set_obj_visible(s_settings_switch_dots[index], false);
    }
}

lv_obj_t *build_settings_menu_label(lv_obj_t *screen,
                                    int x,
                                    int y,
                                    int width,
                                    int index,
                                    SettingsMenuColumn column)
{
    lv_obj_t *label = make_label(screen,
                                 x,
                                 y,
                                 width,
                                 settings_layout::kSettingsSecondaryH,
                                 kSettingsLabelPlaceholder);
    if (!label) {
        if (column == kSettingsMenuPrimaryColumn) {
            ESP_LOGW(TAG, SETTINGS_PRIMARY_LABEL_CREATE_FAILED_FORMAT, index);
        } else {
            ESP_LOGW(TAG, SETTINGS_SECONDARY_LABEL_CREATE_FAILED_FORMAT, index);
        }
        return nullptr;
    }
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    center_align_label(label);
    if(column==kSettingsMenuPrimaryColumn) settings_attach_category_marker(label);
    return label;
}

}

static void style_settings_item(lv_obj_t *label, bool selected, bool primary=false)
{
    settings_visual_style(label, selected, primary);
}

static void style_settings_switch_dot(lv_obj_t *dot, bool on, bool selected)
{
    if (!dot) {
        return;
    }
    lv_color_t fg = selected ? lv_color_white() : lv_color_black();
    lv_color_t bg = selected ? lv_color_black() : lv_color_white();
    lv_obj_set_style_bg_color(dot, on ? fg : bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(dot, fg, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
}

void build_settings_page()
{
    if (auxiliary_page_root(AuxiliaryPage::kSettings)) {
        return;
    }
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 32 * 1024) {
        ESP_LOGW(TAG, "settings creation deferred: LVGL free=%u largest=%u",
                 (unsigned)memory.free_size, (unsigned)memory.free_biggest_size);
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_auxiliary_page_root(AuxiliaryPage::kSettings, screen);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);

    make_centered_label(screen,
                        24,
                        18,
                        352,
                        28,
                        "设置",
                        "settings title create failed");
    make_black_bar(screen, 24, 52, 352, 3);

    make_black_bar(screen, 136, 62, 2, 174);

    for (int i = 0; i < kSettingsPrimaryCount; ++i) {
        s_settings_labels[i] = build_settings_menu_label(screen,
                                                         settings_layout::kSettingsPrimaryX,
                                                         settings_layout::kSettingsListRowY[i],
                                                         settings_layout::kSettingsPrimaryW,
                                                         i,
                                                         kSettingsMenuPrimaryColumn);
    }
    for (int i = 0; i < kSettingsSecondaryMaxCount; ++i) {
        int slot = kSettingsPrimaryCount + i;
        s_settings_labels[slot] = build_settings_menu_label(screen,
                                                            settings_layout::kSettingsSecondaryX,
                                                            settings_layout::kSettingsListRowY[i],
                                                            settings_layout::kSettingsSecondaryW,
                                                            i,
                                                            kSettingsMenuSecondaryColumn);
        s_settings_switch_dots[i] = lv_obj_create(screen);
        if (s_settings_switch_dots[i]) {
            lv_obj_clear_flag(s_settings_switch_dots[i], LV_OBJ_FLAG_SCROLLABLE);
            set_obj_box(s_settings_switch_dots[i],
                        settings_layout::kSettingsSwitchDotX,
                        settings_layout::kSettingsListRowY[i] +
                            settings_layout::kSettingsSwitchDotYOffset,
                        settings_layout::kSettingsSwitchDotSize,
                        settings_layout::kSettingsSwitchDotSize);
            style_settings_switch_dot(s_settings_switch_dots[i], false, false);
            lv_obj_add_flag(s_settings_switch_dots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            ESP_LOGW(TAG, SETTINGS_SWITCH_DOT_CREATE_FAILED_FORMAT, i);
        }
    }
    build_settings_ota_panel(screen,
                             settings_layout::kSettingsSecondaryX,
                             settings_layout::kSettingsSecondaryW);

    s_settings_feedback_label = make_centered_label(screen,
                                                    24,
                                                    246,
                                                    352,
                                                    20,
                                                    "",
                                                    "settings feedback label create failed");

    make_centered_label(screen,
                        24,
                        270,
                        352,
                        22,
                        "KEY选择  长按返回  BOOT确认",
                        "settings hint label create failed");
}

namespace {
bool settings_render_selection_changed(int primary,
                                       int selected,
                                       const SettingsNavigationSnapshot &navigation,
                                       const OtaRuntimeSnapshot &ota)
{
    SettingsRenderCache &cache = settings_render_cache();
    lv_obj_t *settings_root = auxiliary_page_root(AuxiliaryPage::kSettings);
    bool selection_changed = settings_root != cache.settings_root ||
                             selected != cache.selected ||
                             primary != cache.primary ||
                             navigation.focus_secondary != cache.focus_secondary ||
                             navigation.page_toggle_mode != cache.page_toggle_mode ||
                             navigation.page_order_mode != cache.page_order_mode ||
                             navigation.page_order_selection != cache.page_order_selection ||
                             ota.state != cache.ota_state ||
                             ota.progress != cache.ota_progress ||
                             ota.speed_kbps != cache.ota_speed;
    if (selection_changed) {
        cache.settings_root = settings_root;
        cache.selected = selected;
        cache.primary = primary;
        cache.focus_secondary = navigation.focus_secondary;
        cache.page_toggle_mode = navigation.page_toggle_mode;
        cache.page_order_mode = navigation.page_order_mode;
        cache.page_order_selection = navigation.page_order_selection;
        cache.ota_state = ota.state;
        cache.ota_progress = ota.progress;
        cache.ota_speed = ota.speed_kbps;
    }
    return selection_changed;
}

bool update_settings_primary_items(int primary, bool selection_changed)
{
    bool changed = false;
    for (int i = 0; i < kSettingsPrimaryCount; ++i) {
        if (s_settings_labels[i]) {
            changed |= set_label_text_if_changed(s_settings_labels[i], kSettingsPrimaryItems[i]);
            if (selection_changed) {
                style_settings_item(s_settings_labels[i], i == primary, true);
            }
        }
    }
    return changed;
}

bool layout_settings_secondary_slot(
    int index,
    int primary,
    int visible_order_count,
    const uint8_t *visible_order_pages,
    const SettingsNavigationSnapshot &navigation,
    char secondary_items[][kSettingsSecondaryTextSize])
{
    int slot = kSettingsPrimaryCount + index;
    if (navigation.page_order_mode || navigation.page_toggle_mode) {
        int manager_item_count = navigation.page_order_mode ? visible_order_count : kWorkPageCount;
        if (index >= manager_item_count) {
            set_obj_visible(s_settings_labels[slot], false);
            hide_settings_switch_slot(index);
            return false;
        }
        settings_layout::GridCell cell = settings_layout::settings_grid_cell(index);
        lv_obj_set_pos(s_settings_labels[slot], cell.x, cell.y);
        lv_obj_set_size(s_settings_labels[slot],
                        settings_layout::kSettingsGridColW,
                        settings_layout::kSettingsSecondaryH);
        if (navigation.page_order_mode) {
            format_secondary_text(secondary_items,
                                  index,
                                  kSettingsPageOrderEntryFormat,
                                  index + 1,
                                  work_page_name(visible_order_pages[index]));
            hide_settings_switch_slot(index);
        } else {
            set_secondary_text(secondary_items, index, work_page_name(index));
            if (s_settings_switch_dots[index]) {
                lv_obj_set_pos(s_settings_switch_dots[index],
                               cell.x + settings_layout::kSettingsGridSwitchDotXOffset,
                               cell.y + settings_layout::kSettingsGridSwitchDotYOffset);
            }
        }
    } else if (primary == kSettingsPrimarySystem || primary == kSettingsPrimaryDisplay) {
        bool grid_item = primary == kSettingsPrimaryDisplay
                             ? index < kDisplaySettingsGridItemCount
                             : index < kSystemSettingsGridItemCount;
        if (grid_item) {
            settings_layout::GridCell cell = settings_layout::settings_grid_cell(index);
            lv_obj_set_pos(s_settings_labels[slot], cell.x, cell.y);
            lv_obj_set_size(s_settings_labels[slot],
                            settings_layout::kSettingsGridColW,
                            settings_layout::kSettingsSecondaryH);
            if (s_settings_switch_dots[index]) {
                lv_obj_set_pos(s_settings_switch_dots[index],
                               cell.x + settings_layout::kSettingsGridSwitchDotXOffset,
                               cell.y + settings_layout::kSettingsGridSwitchDotYOffset);
            }
        } else {
            lv_obj_set_pos(s_settings_labels[slot],
                           settings_layout::kSettingsSecondaryX,
                           settings_long_item_y(primary));
            lv_obj_set_size(s_settings_labels[slot],
                            settings_layout::kSettingsSecondaryW,
                            settings_layout::kSettingsSecondaryH);
            hide_settings_switch_slot(index);
        }
    } else {
        lv_obj_set_pos(s_settings_labels[slot],
                       settings_layout::kSettingsSecondaryX,
                       settings_layout::kSettingsListRowY[index]);
        lv_obj_set_size(s_settings_labels[slot],
                        settings_layout::kSettingsSecondaryW,
                        settings_layout::kSettingsSecondaryH);
        if (s_settings_switch_dots[index]) {
            lv_obj_set_pos(s_settings_switch_dots[index],
                           settings_layout::kSettingsSwitchDotX,
                           settings_layout::kSettingsListRowY[index] +
                               settings_layout::kSettingsSwitchDotYOffset);
        }
    }
    return true;
}

void update_settings_switch_slot(int index,
                                 int primary,
                                 int selected,
                                 bool visible,
                                 const SettingsNavigationSnapshot &navigation,
                                 const SettingsSecondaryStateSnapshot &secondary_state)
{
    bool dot_visible = false;
    bool dot_on = false;
    if (visible && primary == kSettingsPrimarySound) {
        if (index >= kSoundSettingsHourlyItem) {
            dot_visible = true;
            dot_on = index == kSoundSettingsHourlyItem
                         ? secondary_state.hourly_chime_enabled
                         : secondary_state.all_day_chime_enabled;
        }
    } else if (visible &&
               primary == kSettingsPrimaryDisplay &&
               navigation.page_toggle_mode) {
        dot_visible = true;
        dot_on = work_page_enabled_in_switch_snapshot(secondary_state, index);
    } else if (visible &&
               primary == kSettingsPrimaryDisplay &&
               !navigation.page_toggle_mode &&
               !navigation.page_order_mode &&
               (index == kDisplaySettingsAlarmItem ||
                index == kDisplaySettingsXiaozhiAutoReturnItem)) {
        dot_visible = true;
        dot_on = index == kDisplaySettingsAlarmItem
                     ? secondary_state.alarm_enabled
                     : secondary_state.xiaozhi_auto_return_enabled;
    }
    if (s_settings_switch_dots[index]) {
        set_obj_visible(s_settings_switch_dots[index], dot_visible);
        if (dot_visible) {
            style_settings_switch_dot(s_settings_switch_dots[index],
                                      dot_on,
                                      navigation.focus_secondary && index == selected);
        }
    }
}

bool update_settings_secondary_items(
    int primary,
    int selected,
    bool selection_changed,
    const SettingsNavigationSnapshot &navigation,
    const SettingsSecondaryStateSnapshot &secondary_state,
    char secondary_items[][kSettingsSecondaryTextSize])
{
    bool changed = false;
    int secondary_count = settings_secondary_count(primary);
    int visible_order_indices[kWorkPageCount] = {};
    uint8_t visible_order_pages[kWorkPageCount] = {};
    int visible_order_count = navigation.page_order_mode
                                  ? collect_visible_work_page_order(visible_order_indices,
                                                                    visible_order_pages,
                                                                    array_count(visible_order_indices),
                                                                    secondary_state.work_page_enabled_mask)
                                  : 0;
    for (int i = 0; i < kSettingsSecondaryMaxCount; ++i) {
        int slot = kSettingsPrimaryCount + i;
        if (!s_settings_labels[slot]) {
            continue;
        }
        if (!layout_settings_secondary_slot(i,
                                            primary,
                                            visible_order_count,
                                            visible_order_pages,
                                            navigation,
                                            secondary_items)) {
            continue;
        }
        bool visible = i < secondary_count;
        if (navigation.page_order_mode) {
            visible = i < visible_order_count;
        } else if (navigation.page_toggle_mode) {
            visible = i < kWorkPageCount;
        }
        set_obj_visible(s_settings_labels[slot], visible);
        if (visible) {
            changed |= set_label_text_if_changed(s_settings_labels[slot], secondary_items[i]);
            if (selection_changed) {
                int order_index = navigation.page_order_mode ? visible_order_indices[i] : -1;
                bool selected_item = navigation.page_order_mode
                                         ? order_index == navigation.page_order_selection
                                     : navigation.page_toggle_mode
                                         ? i == selected
                                         : (navigation.focus_secondary && i == selected);
                style_settings_item(s_settings_labels[slot], selected_item);
                const bool compact_grid_item =
                    (primary == kSettingsPrimarySystem && i < kSystemSettingsGridItemCount) ||
                    (primary == kSettingsPrimaryDisplay && i < kDisplaySettingsGridItemCount);
                if (compact_grid_item) {
                    const bool display_switch_item =
                        primary == kSettingsPrimaryDisplay &&
                        !navigation.page_toggle_mode &&
                        !navigation.page_order_mode &&
                        i == kDisplaySettingsAlarmItem;
                    lv_obj_set_style_pad_left(
                        s_settings_labels[slot],
                        display_switch_item
                            ? settings_layout::kSettingsGridSwitchLabelLeftPadding
                            : settings_layout::kSettingsGridLabelPadding,
                        LV_PART_MAIN);
                    lv_obj_set_style_pad_right(
                        s_settings_labels[slot],
                        display_switch_item
                            ? settings_layout::kSettingsGridSwitchLabelRightPadding
                            : settings_layout::kSettingsGridLabelPadding,
                        LV_PART_MAIN);
                }
            }
        }
        update_settings_switch_slot(
            i, primary, selected, visible, navigation, secondary_state);
    }
    return changed;
}

bool update_settings_feedback_label()
{
    if (!s_settings_feedback_label) {
        return false;
    }
    TickType_t now = xTaskGetTickCount();
    SettingsFeedbackSnapshot feedback;
    if (!settings_feedback_snapshot_load(now, &feedback)) {
        return false;
    }
    return set_label_text_if_changed(
        s_settings_feedback_label,
        feedback.active ? feedback.text : "");
}
} // namespace

bool update_settings_page()
{
    ota_reset_status_if_idle();
    OtaRuntimeSnapshot ota = {};
    ota_runtime_snapshot_load(&ota);
    SettingsRenderWorkspace &workspace = s_settings_render_workspace;
    memset(&workspace, 0, sizeof(workspace));
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    int primary = clamp_settings_primary(navigation.primary_selection);
    int selected = clamp_settings_selection_for_mode(primary,
                                                     navigation.selection,
                                                     navigation.page_toggle_mode);
    if (navigation.page_order_mode) {
        navigation.page_order_selection =
            valid_enabled_work_page_order_index(navigation.page_order_selection);
    }
    const SettingsSecondaryStateSnapshot secondary_state =
        settings_secondary_state_snapshot(primary, navigation);

    if (!navigation.page_order_mode && !navigation.page_toggle_mode) {
        populate_settings_secondary_items(
            primary, secondary_state, workspace.secondary_items);
    }
    bool selection_changed =
        settings_render_selection_changed(primary, selected, navigation, ota);
    bool changed = selection_changed;
    changed |= update_settings_primary_items(primary, selection_changed);
    changed |= update_settings_secondary_items(primary,
                                               selected,
                                               selection_changed,
                                               navigation,
                                               secondary_state,
                                               workspace.secondary_items);
    bool ota_panel_visible = primary == kSettingsPrimarySystem && selected == kSystemSettingsOtaItem;
    changed |= update_settings_ota_panel(ota_panel_visible, ota);
    changed |= update_settings_feedback_label();
    return changed;
}

void clear_settings_page_object_refs()
{
    for (lv_obj_t *&label : s_settings_labels) {
        label = nullptr;
    }
    for (lv_obj_t *&dot : s_settings_switch_dots) {
        dot = nullptr;
    }
    s_settings_feedback_label = nullptr;
    reset_settings_render_cache();
}
