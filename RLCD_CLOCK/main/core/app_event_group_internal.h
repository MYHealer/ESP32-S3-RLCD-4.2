// 仅向应用启动所有者开放全局事件组的创建和失败回滚接口。
#pragma once

#include "app_event_group.h"

bool app_event_group_init();
void app_event_group_release();
