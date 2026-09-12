// 声明仅供配网事务和底层所有者使用的完成写入口。
#pragma once

#include "setup_portal_control.h"

void complete_setup_portal_stop_request();
bool prepare_setup_portal_result_delivery();
