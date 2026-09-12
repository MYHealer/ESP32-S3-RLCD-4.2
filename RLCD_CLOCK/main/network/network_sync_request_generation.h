// 跟踪用户触发联网请求的代次，避免旧任务清除同类型新请求。
#pragma once

#include "app_event_group.h"

#include <stdint.h>

struct NetworkSyncRequestGenerationSnapshot {
    uint32_t manual_ntp = 0;
    uint32_t manual_weather = 0;
    uint32_t manual_saying = 0;
    uint32_t diagnostics = 0;
};

NetworkSyncRequestGenerationSnapshot
network_sync_request_generation_snapshot();

// 仅接受一个受跟踪请求位；成功时发布事件并返回非零代次。
uint32_t publish_network_sync_request(EventBits_t request_bit);

// 在同一生命周期锁内核对事件位和代次，供阻塞操作前后确认在途任务
// 仍拥有请求；取消后重新发布的同类型请求不会被旧任务误认领。
bool network_sync_request_is_current(EventBits_t request_bit,
                                     uint32_t expected_generation);

// 只结算仍属于 expected_generation 的请求。若期间已有新请求，
// 保留新事件位并返回 false。
bool retire_network_sync_request(EventBits_t request_bit,
                                 uint32_t expected_generation);

// 先推进所含受跟踪请求位的代次，再清理事件位，使在途快照失效。
void invalidate_network_sync_requests(EventBits_t request_bits);
