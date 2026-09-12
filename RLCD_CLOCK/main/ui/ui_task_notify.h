// 声明 UI 任务句柄发布和跨任务通知接口。
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void register_ui_task_handle(TaskHandle_t handle);
void notify_ui_task();
