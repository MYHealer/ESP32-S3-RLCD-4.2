// 声明网络配置、配网页提交与事件适配模块共享的安全 EventGroup 操作。
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

void clear_config_event_bits(EventBits_t bits, const char *reason);
void set_config_event_bits(EventBits_t bits, const char *reason);
