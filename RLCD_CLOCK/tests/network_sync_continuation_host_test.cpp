// 验证多段联网同步在运行态变化后停止发起后续请求。
#include "network_sync_runtime.h"

#include "network_credentials_state.h"
#include "network_sync_schedule.h"
#include "ota_flow_policy.h"

#include <assert.h>

namespace {
bool g_wifi_configured = true;
bool g_weather_api_key_configured = true;
bool g_weather_api_host_configured = true;
bool g_offline_mode = false;
bool g_low_battery_mode = false;
int g_ota_state = kOtaIdle;
bool g_setup_portal_start_requested = false;

void reset_runtime()
{
    g_wifi_configured = true;
    g_weather_api_key_configured = true;
    g_weather_api_host_configured = true;
    g_offline_mode = false;
    g_low_battery_mode = false;
    g_ota_state = kOtaIdle;
    g_setup_portal_start_requested = false;
}
} // namespace

NetworkCredentialsAvailability network_credentials_availability()
{
    return {
        g_wifi_configured,
        g_weather_api_key_configured,
        g_weather_api_host_configured,
    };
}

bool offline_mode_enabled_load()
{
    return g_offline_mode;
}

bool battery_low_mode_load()
{
    return g_low_battery_mode;
}

int ota_runtime_state_load()
{
    return g_ota_state;
}

bool setup_portal_start_requested()
{
    return g_setup_portal_start_requested;
}

int main()
{
    reset_runtime();
    NetworkSyncAvailability availability =
        capture_network_runtime_availability();
    assert(availability.have_wifi_creds);
    assert(availability.have_weather_config);
    assert(!availability.offline_mode);
    assert(!availability.low_battery_mode);
    assert(!network_sync_start_context_changed(availability, availability));

    g_wifi_configured = false;
    NetworkSyncAvailability changed = capture_network_runtime_availability();
    assert(!changed.have_wifi_creds);
    assert(network_sync_start_context_changed(availability, changed));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_weather_api_host_configured = false;
    changed = capture_network_runtime_availability();
    assert(!changed.have_weather_config);
    assert(network_sync_start_context_changed(availability, changed));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_weather_api_key_configured = false;
    changed = capture_network_runtime_availability();
    assert(!changed.have_weather_config);
    assert(network_sync_start_context_changed(availability, changed));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_offline_mode = true;
    changed = capture_network_runtime_availability();
    assert(changed.offline_mode);
    assert(network_sync_start_context_changed(availability, changed));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_low_battery_mode = true;
    changed = capture_network_runtime_availability();
    assert(changed.low_battery_mode);
    assert(network_sync_start_context_changed(availability, changed));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_ota_state = kOtaChecking;
    assert(network_sync_start_context_changed(availability, availability));

    reset_runtime();
    availability = capture_network_runtime_availability();
    g_setup_portal_start_requested = true;
    assert(network_sync_start_context_changed(availability, availability));

    NetworkSyncContinuationState state = {};
    assert(network_sync_continuation_allowed(state));

    state.offline_mode = true;
    assert(!network_sync_continuation_allowed(state));
    state = {};
    state.low_battery_mode = true;
    assert(!network_sync_continuation_allowed(state));
    state = {};
    state.ota_blocks_background_sync = true;
    assert(!network_sync_continuation_allowed(state));
    state = {};
    state.setup_portal_start_requested = true;
    assert(!network_sync_continuation_allowed(state));

    reset_runtime();
    assert(network_sync_continuation_allowed());
    g_offline_mode = true;
    assert(!network_sync_continuation_allowed());

    reset_runtime();
    g_low_battery_mode = true;
    assert(!network_sync_continuation_allowed());

    reset_runtime();
    g_ota_state = kOtaChecking;
    assert(!network_sync_continuation_allowed());
    g_ota_state = kOtaUpdating;
    assert(!network_sync_continuation_allowed());
    g_ota_state = kOtaAvailable;
    assert(network_sync_continuation_allowed());

    reset_runtime();
    g_setup_portal_start_requested = true;
    assert(!network_sync_continuation_allowed());
    return 0;
}
