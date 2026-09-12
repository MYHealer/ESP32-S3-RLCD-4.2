// 声明网络同步请求快照、只读资格和公共取消/清理入口。
#pragma once

#include "ui_settings_contract.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

struct NetworkSyncRequestSnapshot {
    bool provisioning = false;
    uint32_t provisioning_generation = 0;
    bool manual_ntp = false;
    uint32_t manual_ntp_generation = 0;
    uint32_t manual_ntp_settings_generation = 0;
    bool manual_weather = false;
    uint32_t manual_weather_generation = 0;
    uint32_t manual_weather_settings_generation = 0;
    bool manual_saying = false;
    uint32_t manual_saying_generation = 0;
    uint32_t manual_saying_settings_generation = 0;
    bool visible_weather = false;
    bool visible_saying = false;
    bool diagnostics = false;
    uint32_t diagnostics_generation = 0;
    uint32_t diagnostics_settings_generation = 0;

    bool weather_due() const
    {
        return manual_weather || visible_weather;
    }

    bool saying_due() const
    {
        return manual_saying || visible_saying;
    }

    // New unrelated requests may join the same window, but a replacement or
    // cancellation of any scheduled request invalidates its ownership.
    bool still_owned_by(const NetworkSyncRequestSnapshot &current) const;
    NetworkSyncRequestSnapshot provisioning_only() const;
};

inline bool NetworkSyncRequestSnapshot::still_owned_by(
    const NetworkSyncRequestSnapshot &current) const
{
    return (!provisioning ||
            (current.provisioning &&
             provisioning_generation == current.provisioning_generation)) &&
           (!manual_ntp ||
            (current.manual_ntp &&
             manual_ntp_generation == current.manual_ntp_generation)) &&
           (!manual_weather ||
            (current.manual_weather &&
             manual_weather_generation ==
                 current.manual_weather_generation)) &&
           (!manual_saying ||
            (current.manual_saying &&
             manual_saying_generation ==
                 current.manual_saying_generation)) &&
           (!visible_weather || current.visible_weather) &&
           (!visible_saying || current.visible_saying) &&
           (!diagnostics ||
            (current.diagnostics &&
             diagnostics_generation == current.diagnostics_generation));
}

inline NetworkSyncRequestSnapshot
NetworkSyncRequestSnapshot::provisioning_only() const
{
    NetworkSyncRequestSnapshot filtered;
    filtered.provisioning = provisioning;
    filtered.provisioning_generation = provisioning_generation;
    return filtered;
}

NetworkSyncRequestSnapshot snapshot_network_sync_requests();
bool network_sync_request_snapshot_still_current(
    const NetworkSyncRequestSnapshot &scheduled);
EventBits_t network_sync_request_bits(const NetworkSyncRequestSnapshot &requests);
void clear_network_request_bits();
bool network_diagnostics_request_pending();
void cancel_network_diagnostics_sync();
