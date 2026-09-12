// 声明关于本机页面仅供 UI 与 OTA 协调者使用的截止时间写入接口。
#pragma once

#include "ui_info_page_state.h"

bool info_page_state_init();
void info_page_hold_until_store(uint32_t hold_until_tick);
