// 声明设置页导航状态的单原子完整快照。
#pragma once

struct SettingsNavigationSnapshot {
    bool focus_secondary = false;
    bool page_toggle_mode = false;
    bool page_order_mode = false;
    int primary_selection = 0;
    int selection = 0;
    int page_order_selection = 0;
};

SettingsNavigationSnapshot settings_navigation_snapshot();
