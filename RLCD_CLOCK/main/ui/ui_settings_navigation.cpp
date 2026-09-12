// 维护设置页 KEY 导航、返回状态和二次确认清理逻辑。
#include "ui_settings_navigation.h"

#include "active_work_page_state_internal.h"
#include "app_tick_time.h"
#include "ui_settings_activity_state.h"
#include "ui_settings_confirmation_state_internal.h"
#include "ui_settings_feedback.h"
#include "ui_settings_layout.h"
#include "ui_settings_navigation_state_internal.h"
#include "ui_task_notify.h"
#include "ui_work_page_catalog.h"
#include "work_page_ids.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>

namespace {
std::atomic<TickType_t> s_settings_primary_exit_block_until{0};

constexpr uint32_t kSettingsPrimaryExitBlockMs = 800;
constexpr uint32_t kSettingsOrderExitFeedbackMs = 2500;
constexpr const char *kSettingsOrderExitSavedFeedback = "页面顺序已保存";

constexpr int clamp_selection_to_count(int selected, int count)
{
    return count > 0 && selected >= 0 && selected < count ? selected : 0;
}

static_assert(kSettingsPrimaryExitBlockMs > 0, "settings primary exit block duration must be positive");
static_assert(kSettingsOrderExitFeedbackMs > 0, "settings order exit feedback duration must be positive");
static_assert(kSettingsOrderExitSavedFeedback[0] != '\0', "settings order saved feedback must not be empty");
static_assert(clamp_selection_to_count(kWorkPageCalendar, kWorkPageCount) == kWorkPageCalendar &&
                  clamp_selection_to_count(kWorkPageHistory, kWorkPageCount) == kWorkPageHistory &&
                  clamp_selection_to_count(kWorkPageXiaozhiAI, kWorkPageCount) == kWorkPageXiaozhiAI,
              "high work page indices must remain selectable in page toggle mode");
static_assert(kSettingsPrimaryCount <= 256 && kSettingsSecondaryMaxCount <= 256 &&
                  kWorkPageCount <= 256,
              "settings navigation selections must fit in the atomic snapshot fields");
} // namespace

namespace settings_layout = ui_settings_layout;

int settings_secondary_count(int primary)
{
    switch (primary) {
    case kSettingsPrimaryNetwork:
        return kNetworkSettingsSecondaryCount;
    case kSettingsPrimarySound:
        return kSoundSettingsSecondaryCount;
    case kSettingsPrimaryDisplay:
        return kDisplaySettingsSecondaryCount;
    case kSettingsPrimarySystem:
        return kSystemSettingsSecondaryCount;
    default:
        return 0;
    }
}

void reset_settings_confirmation()
{
    settings_confirmation_clear_all();
}

void reset_settings_navigation_state()
{
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    navigation.focus_secondary = false;
    navigation.page_toggle_mode = false;
    navigation.page_order_mode = false;
    settings_navigation_store(navigation);
    s_settings_primary_exit_block_until.store(0, std::memory_order_relaxed);
    reset_settings_confirmation();
}

void enter_settings_primary_navigation()
{
    SettingsNavigationSnapshot navigation;
    navigation.primary_selection = kSettingsPrimaryNetwork;
    settings_navigation_store(navigation);
    s_settings_primary_exit_block_until.store(0, std::memory_order_relaxed);
}

void enter_settings_system_item_navigation(int selection)
{
    const int selected = clamp_settings_secondary(kSettingsPrimarySystem, selection);
    SettingsNavigationSnapshot navigation;
    navigation.focus_secondary = true;
    navigation.primary_selection = kSettingsPrimarySystem;
    navigation.selection = selected;
    settings_navigation_store(navigation);
}

int clamp_settings_primary(int primary)
{
    if (primary < 0 || primary >= kSettingsPrimaryCount) {
        return kSettingsPrimaryNetwork;
    }
    return primary;
}

int clamp_settings_secondary(int primary, int selected)
{
    int count = settings_secondary_count(primary);
    return clamp_selection_to_count(selected, count);
}

int clamp_settings_selection_for_mode(int primary, int selected, bool page_toggle_mode)
{
    if (!page_toggle_mode) {
        return clamp_settings_secondary(primary, selected);
    }
    return clamp_selection_to_count(selected, kWorkPageCount);
}

void handle_settings_key_short(int direction)
{
    settings_activity_record(xTaskGetTickCount());
    const int step = direction >= 0 ? 1 : -1;
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    int primary = clamp_settings_primary(navigation.primary_selection);
    navigation.primary_selection = primary;
    if (navigation.page_order_mode) {
        // 页面顺序模式移动的是"要排序哪一项"的游标，范围固定为页表长度，
        // 真正的顺序重排在确认时由 swap 完成，与游标无关。
        navigation.page_order_selection =
            (valid_enabled_work_page_order_index(navigation.page_order_selection) +
             step + kWorkPageCount) %
            kWorkPageCount;
    } else if (navigation.page_toggle_mode) {
        navigation.selection =
            (navigation.selection + step + kWorkPageCount) % kWorkPageCount;
    } else if (navigation.focus_secondary) {
        int count = settings_secondary_count(primary);
        if (count > 0) {
            navigation.selection =
                (clamp_settings_secondary(primary, navigation.selection) + step +
                 count) %
                count;
        }
    } else {
        navigation.primary_selection =
            (primary + step + kSettingsPrimaryCount) % kSettingsPrimaryCount;
        navigation.selection = 0;
    }
    settings_navigation_store(navigation);
    reset_settings_confirmation();
    clear_settings_feedback();
    notify_ui_task();
}

// 二级菜单是否为 2 列网格布局。Display/System 的前若干项按
// index = row * kSettingsGridColumns + col 排布，其余菜单是单列列表。
// 页面开关 / 页面顺序两个子模式固定走 settings_grid_cell()，也是 2 列。
int settings_secondary_columns(int primary, int count)
{
    const int grid_items = primary == kSettingsPrimaryDisplay
                               ? kDisplaySettingsGridItemCount
                               : primary == kSettingsPrimarySystem
                                     ? kSystemSettingsGridItemCount
                                     : 0;
    if (grid_items <= 0 || grid_items > count) {
        return 1;
    }
    return settings_layout::kSettingsGridColumns;
}

// 把二维移动换算成一维索引步长。网格里"上下换一行"= ±列数，单列时退化为 ±1。
int focus_step(int row_step, int col_step, int columns)
{
    return col_step + row_step * columns;
}

void move_settings_focus(int row_step, int col_step)
{
    if (row_step == 0 && col_step == 0) {
        return;
    }
    settings_activity_record(xTaskGetTickCount());
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    const int primary = clamp_settings_primary(navigation.primary_selection);
    navigation.primary_selection = primary;

    if (navigation.page_order_mode) {
        // 页面开关/顺序两个子模式都按 settings_grid_cell() 排成 2 列，
        // 所以"上下"是 ±2 而不是 ±1（否则表现为只递增递减）。
        const int step =
            focus_step(row_step, col_step, settings_layout::kSettingsGridColumns);
        navigation.page_order_selection =
            (valid_enabled_work_page_order_index(navigation.page_order_selection) +
             step % kWorkPageCount + kWorkPageCount) %
            kWorkPageCount;
    } else if (navigation.page_toggle_mode) {
        const int step =
            focus_step(row_step, col_step, settings_layout::kSettingsGridColumns);
        navigation.selection =
            (navigation.selection + step % kWorkPageCount + kWorkPageCount) %
            kWorkPageCount;
    } else if (!navigation.focus_secondary) {
        // 主菜单是左侧单列：上下和左右都切主菜单项，避免右键无响应。
        const int step = col_step != 0 ? col_step : row_step;
        navigation.primary_selection =
            (primary + step + kSettingsPrimaryCount) % kSettingsPrimaryCount;
        navigation.selection = 0;
    } else {
        const int count = settings_secondary_count(primary);
        if (count <= 0) {
            return;
        }
        const int columns = settings_secondary_columns(primary, count);
        const int current =
            clamp_settings_secondary(primary, navigation.selection);
        // 网格里"上下移一行"= ±columns；单列时 columns=1 退化为 ±1。
        const int step = focus_step(row_step, col_step, columns);
        navigation.selection =
            (current + step % count + count) % count;
    }
    settings_navigation_store(navigation);
    reset_settings_confirmation();
    clear_settings_feedback();
    notify_ui_task();
}

void handle_settings_key_long()
{
    settings_activity_record(xTaskGetTickCount());
    SettingsNavigationSnapshot navigation = settings_navigation_snapshot();
    if (navigation.page_order_mode) {
        navigation.page_order_mode = false;
        navigation.focus_secondary = true;
        navigation.primary_selection = kSettingsPrimaryDisplay;
        navigation.selection = kDisplaySettingsOrderItem;
        settings_navigation_store(navigation);
        active_work_page_store(first_enabled_work_page());
        set_settings_feedback(kSettingsOrderExitSavedFeedback, kSettingsOrderExitFeedbackMs);
        reset_settings_confirmation();
        notify_ui_task();
        return;
    } else if (navigation.page_toggle_mode) {
        navigation.page_toggle_mode = false;
        navigation.focus_secondary = true;
        navigation.primary_selection = kSettingsPrimaryDisplay;
        navigation.selection = kDisplaySettingsPageSwitchItem;
        settings_navigation_store(navigation);
        reset_settings_confirmation();
        clear_settings_feedback();
        notify_ui_task();
        return;
    } else if (navigation.focus_secondary) {
        navigation.focus_secondary = false;
        navigation.selection = 0;
        settings_navigation_store(navigation);
        s_settings_primary_exit_block_until.store(
            xTaskGetTickCount() + pdMS_TO_TICKS(kSettingsPrimaryExitBlockMs),
            std::memory_order_relaxed);
    } else {
        TickType_t now = xTaskGetTickCount();
        const TickType_t exit_block_until =
            s_settings_primary_exit_block_until.load(std::memory_order_relaxed);
        if (exit_block_until != 0 &&
            app_tick_deadline_pending(now, exit_block_until)) {
            settings_activity_record(now);
            notify_ui_task();
            return;
        }
        s_settings_primary_exit_block_until.store(0, std::memory_order_relaxed);
        settings_page_clear();
        reset_settings_navigation_state();
        clear_settings_feedback();
        notify_ui_task();
        return;
    }
    reset_settings_confirmation();
    clear_settings_feedback();
    notify_ui_task();
}
