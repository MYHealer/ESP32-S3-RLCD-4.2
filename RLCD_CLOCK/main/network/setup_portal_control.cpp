// 负责配网热点启停请求的跨任务发布、查询和日志。
#include "setup_portal_control.h"
#include "setup_portal_control_internal.h"

#include "app_event_group.h"
#include "app_metadata.h"

#include <atomic>
#include <esp_log.h>

namespace {
std::atomic<bool> s_setup_portal_stop_requested{false};
constexpr const char *kSetupPortalStartRequestedLog =
    "setup portal start queued behind active network work";
constexpr const char *kSetupPortalStopRequestedLog =
    "setup portal stop queued after offline save";
} // namespace

bool request_setup_portal_start()
{
    if (!app_event_group_ready()) {
        return false;
    }
    app_event_group_set_bits(kSetupPortalStartBit | kNetworkStateChangedBit);
    ESP_LOGI(TAG, "%s", kSetupPortalStartRequestedLog);
    return true;
}

bool setup_portal_start_requested()
{
    return app_event_group_ready() &&
           (app_event_group_get_bits() & kSetupPortalStartBit) != 0;
}

bool request_setup_portal_stop()
{
    if (!app_event_group_ready()) {
        return false;
    }
    s_setup_portal_stop_requested.store(true, std::memory_order_release);
    app_event_group_set_bits(kNetworkStateChangedBit);
    ESP_LOGI(TAG, "%s", kSetupPortalStopRequestedLog);
    return true;
}

bool setup_portal_stop_requested()
{
    return s_setup_portal_stop_requested.load(std::memory_order_acquire);
}

void complete_setup_portal_stop_request()
{
    s_setup_portal_stop_requested.store(false, std::memory_order_release);
}
