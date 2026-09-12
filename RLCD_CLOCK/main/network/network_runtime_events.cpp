// 发布只用于唤醒后台网络调度器的运行态变化事件。
#include "network_runtime_events.h"

#include "app_event_group.h"

bool cancel_pending_network_sync_requests(uint32_t request_bits)
{
    if (request_bits == 0) {
        return false;
    }
    const EventBits_t bits = static_cast<EventBits_t>(request_bits);
    const EventBits_t previous_bits = app_event_group_clear_bits(bits);
    if ((previous_bits & bits) == 0) {
        return false;
    }
    notify_network_sync_runtime_state_changed();
    return true;
}

void notify_network_sync_runtime_state_changed()
{
    if (app_event_group_ready()) {
        app_event_group_set_bits(kNetworkStateChangedBit);
    }
}
