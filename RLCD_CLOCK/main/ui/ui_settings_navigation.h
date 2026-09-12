// 声明设置页菜单数量、确认状态和 KEY 导航接口。
#pragma once

#include "ui_settings_contract.h"
#include "ui_settings_navigation_state.h"

int settings_secondary_count(int primary);
int clamp_settings_primary(int primary);
int clamp_settings_secondary(int primary, int selected);
int clamp_settings_selection_for_mode(int primary, int selected, bool page_toggle_mode);
void reset_settings_confirmation();
void reset_settings_navigation_state();
void enter_settings_primary_navigation();
void enter_settings_system_item_navigation(int selection);
// 在设置页内移动选择。direction > 0 下一项，direction < 0 上一项。
// 设备 KEY 短按（单向）传 +1；遥控上下键传 ±1。
// 四种模式（页面顺序 / 页面开关 / 二级菜单 / 主菜单）都按模运算循环，
// 双向不需要额外的 prev 语义。
void handle_settings_key_short(int direction);
void handle_settings_key_long();
// 遥控器专用二维光���移动。row_step/col_step 取 -1/0/+1。
// 设置页二级菜单布局不统一：Display/System 是 2 列网格（index = row*2 + col），
// Network/Sound 是单列列表，页面开关/顺序模式是 8 项列表。因此"上下移一行"
// 的步长必须由布局决定（网格 ±2、列表 ±1），不能硬编码。
// 只移动焦点，不执行任何动作——"选择/执行"归确认键（OK/BOOT）。
void move_settings_focus(int row_step, int col_step);
