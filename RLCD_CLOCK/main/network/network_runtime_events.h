// 发布只用于唤醒后台网络调度器的运行态变化事件。
#pragma once

#include <stdint.h>

bool cancel_pending_network_sync_requests(uint32_t request_bits);
void notify_network_sync_runtime_state_changed();
