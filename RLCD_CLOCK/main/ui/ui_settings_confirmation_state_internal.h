// 声明仅供设置动作与导航所有者使用的二次确认写入口。
#pragma once

#include "ui_settings_confirmation_state.h"

void settings_confirmation_request(SettingsConfirmation confirmation);
void settings_confirmation_clear(SettingsConfirmation confirmation);
void settings_confirmation_clear_all();
