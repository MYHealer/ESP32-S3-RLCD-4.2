// 验证配网页会话状态、Wi-Fi 断线原因、AP 名称和本地 IP 的跨任务快照。
#include "wifi_portal_state_internal.h"
#include "app_event_group.h"

#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <thread>

namespace {
bool s_event_group_ready = true;
EventBits_t s_event_bits = 0;
}

bool app_event_group_ready()
{
    return s_event_group_ready;
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    s_event_bits |= bits;
    return s_event_bits;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    EventBits_t previous = s_event_bits;
    s_event_bits &= ~bits;
    return previous;
}

int main()
{
    assert(!setup_portal_active_load());
    s_event_bits = 0;
    setup_portal_active_store(false);
    assert(!setup_portal_active_load());
    assert(s_event_bits == 0);

    setup_portal_active_store(true);
    assert(setup_portal_active_load());
    setup_portal_active_store(false);
    assert(!setup_portal_active_load());
    assert((s_event_bits & kProvisioningFeedbackBit) != 0);
    assert((s_event_bits & kNetworkStateChangedBit) != 0);
    s_event_bits = 0;

    assert(wifi_last_disconnect_reason() == 0);
    record_wifi_disconnect_reason(8);
    assert(wifi_last_disconnect_reason() == 8);
    record_wifi_disconnect_reason(-1);
    assert(wifi_last_disconnect_reason() == -1);
    clear_wifi_last_disconnect_reason();
    assert(wifi_last_disconnect_reason() == 0);

    assert(wifi_portal_save_result_load() == WifiPortalSaveResult::kNone);
    assert(!wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kNone));
    assert(wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kValidating));
    assert(wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kSuccess));
    assert(!wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kWifiConnectionFailed));
    assert(!wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kWeatherApiFailed));
    assert(!wifi_portal_result_preserves_client_lease(
        WifiPortalSaveResult::kWeatherCityInvalid));
    wifi_portal_save_result_store(WifiPortalSaveResult::kValidating);
    assert(wifi_portal_save_result_load() == WifiPortalSaveResult::kValidating);
    wifi_portal_save_result_store(WifiPortalSaveResult::kWifiConnectionFailed);
    assert(wifi_portal_save_result_load() ==
           WifiPortalSaveResult::kWifiConnectionFailed);
    wifi_portal_save_result_store(WifiPortalSaveResult::kWeatherApiFailed);
    assert(wifi_portal_save_result_load() == WifiPortalSaveResult::kWeatherApiFailed);
    wifi_portal_save_result_store(WifiPortalSaveResult::kNone);

    assert(!wifi_portal_save_feedback_seen_load());
    wifi_portal_save_feedback_seen_store(true);
    assert(wifi_portal_save_feedback_seen_load());
    assert((s_event_bits & kProvisioningFeedbackBit) != 0);
    wifi_portal_save_feedback_seen_store(false);
    assert(!wifi_portal_save_feedback_seen_load());
    assert((s_event_bits & kProvisioningFeedbackBit) == 0);

    wifi_portal_save_result_store(WifiPortalSaveResult::kSuccess);
    const WifiPortalSaveSnapshot stale_success =
        wifi_portal_save_snapshot_load();
    assert(stale_success.result == WifiPortalSaveResult::kSuccess);
    assert(!stale_success.feedback_seen);
    s_event_bits = 0;
    const WifiPortalSaveSnapshot next_attempt =
        wifi_portal_begin_save_attempt();
    assert(next_attempt.result == WifiPortalSaveResult::kNone);
    assert(!next_attempt.feedback_seen);
    assert(next_attempt.generation != stale_success.generation);
    assert((s_event_bits & kProvisioningFeedbackBit) != 0);
    assert(!wifi_portal_mark_save_feedback_seen(stale_success));
    assert(!wifi_portal_save_feedback_seen_load());

    wifi_portal_save_result_store(WifiPortalSaveResult::kSuccess);
    const WifiPortalSaveSnapshot current_success =
        wifi_portal_save_snapshot_load();
    assert(current_success.generation == next_attempt.generation);
    WifiPortalSaveSnapshot ignored_publish = {};
    assert(!wifi_portal_save_result_store_if_generation(
        stale_success.generation,
        WifiPortalSaveResult::kWeatherApiFailed,
        &ignored_publish));
    assert(wifi_portal_save_result_load() == WifiPortalSaveResult::kSuccess);
    assert(wifi_portal_mark_save_feedback_seen(current_success));
    assert(wifi_portal_save_feedback_seen_load());
    WifiPortalSaveSnapshot current_publish = {};
    assert(wifi_portal_save_result_store_if_generation(
        current_success.generation,
        WifiPortalSaveResult::kWeatherApiFailed,
        &current_publish));
    assert(current_publish.generation == current_success.generation);
    assert(current_publish.result == WifiPortalSaveResult::kWeatherApiFailed);
    assert(!current_publish.feedback_seen);
    wifi_portal_save_feedback_seen_store(false);

    wifi_portal_session_reset();
    setup_portal_active_store(true);
    assert(wifi_portal_should_restart_dhcp());
    assert(wifi_portal_ap_client_connected(4) == 1);
    assert(wifi_portal_ap_client_connected(4) == 2);
    assert(wifi_portal_ap_client_connected(4) == 3);
    assert(wifi_portal_ap_client_connected(4) == 4);
    assert(wifi_portal_ap_client_connected(4) == 4);
    assert(!wifi_portal_should_restart_dhcp());
    assert(wifi_portal_ap_client_disconnected() == 3);
    assert(wifi_portal_ap_client_disconnected() == 2);
    assert(wifi_portal_ap_client_disconnected() == 1);
    assert(wifi_portal_ap_client_disconnected() == 0);
    assert(wifi_portal_ap_client_disconnected() == 0);
    assert(wifi_portal_should_restart_dhcp());

    wifi_portal_ap_channel_transition_begin();
    assert(!wifi_portal_should_restart_dhcp());
    wifi_portal_ap_channel_transition_end();
    assert(wifi_portal_should_restart_dhcp());

    wifi_portal_save_result_store(WifiPortalSaveResult::kValidating);
    assert(!wifi_portal_should_restart_dhcp());
    wifi_portal_save_result_store(WifiPortalSaveResult::kSuccess);
    assert(!wifi_portal_should_restart_dhcp());
    wifi_portal_save_result_store(WifiPortalSaveResult::kWifiConnectionFailed);
    assert(wifi_portal_should_restart_dhcp());
    setup_portal_active_store(false);
    assert(!wifi_portal_should_restart_dhcp());

    wifi_portal_ap_client_connected(4);
    wifi_portal_ap_channel_transition_begin();
    wifi_portal_save_result_store(WifiPortalSaveResult::kSuccess);
    wifi_portal_save_feedback_seen_store(true);
    s_event_bits = 0;
    wifi_portal_session_reset();
    assert((s_event_bits & kProvisioningFeedbackBit) == 0);
    setup_portal_active_store(true);
    assert(wifi_portal_should_restart_dhcp());
    assert(wifi_portal_save_result_load() == WifiPortalSaveResult::kNone);
    assert(!wifi_portal_save_feedback_seen_load());
    setup_portal_active_store(false);

    char station_ip[kWifiStationIpTextLen] = {};
    char setup_ap_ssid[kWifiSetupApSsidTextLen] = {};
    assert(!wifi_station_ip_snapshot(station_ip, sizeof(station_ip)));
    assert(!wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid)));
    wifi_setup_ap_ssid_store("ignored-before-init");
    assert(wifi_portal_state_init());
    assert(wifi_portal_state_init());
    assert(!wifi_station_ip_snapshot(station_ip, sizeof(station_ip)));
    assert(station_ip[0] == '\0');
    assert(!wifi_station_ip_snapshot(station_ip, sizeof(station_ip) - 1));
    assert(!wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid)));
    assert(setup_ap_ssid[0] == '\0');
    assert(!wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid) - 1));

    wifi_setup_ap_ssid_store("WeatherClock-A1B2");
    assert(wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid)));
    assert(std::strcmp(setup_ap_ssid, "WeatherClock-A1B2") == 0);

    wifi_station_ip_store("192.168.4.20");
    assert(wifi_station_ip_snapshot(station_ip, sizeof(station_ip)));
    assert(std::strcmp(station_ip, "192.168.4.20") == 0);

    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            wifi_station_ip_store((i & 1) ? "10.10.10.155" : "192.168.100.200");
        }
        writer_done.store(true, std::memory_order_release);
    });
    while (!writer_done.load(std::memory_order_acquire)) {
        assert(wifi_station_ip_snapshot(station_ip, sizeof(station_ip)));
        assert(std::strcmp(station_ip, "10.10.10.155") == 0 ||
               std::strcmp(station_ip, "192.168.100.200") == 0 ||
               std::strcmp(station_ip, "192.168.4.20") == 0);
    }
    writer.join();

    writer_done.store(false, std::memory_order_release);
    std::thread ap_writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            wifi_setup_ap_ssid_store((i & 1) ? "WeatherClock-1234" :
                                                 "WeatherClock-ABCD");
        }
        writer_done.store(true, std::memory_order_release);
    });
    while (!writer_done.load(std::memory_order_acquire)) {
        assert(wifi_setup_ap_ssid_snapshot(setup_ap_ssid, sizeof(setup_ap_ssid)));
        assert(std::strcmp(setup_ap_ssid, "WeatherClock-1234") == 0 ||
               std::strcmp(setup_ap_ssid, "WeatherClock-ABCD") == 0 ||
               std::strcmp(setup_ap_ssid, "WeatherClock-A1B2") == 0);
    }
    ap_writer.join();

    clear_wifi_station_ip();
    assert(!wifi_station_ip_snapshot(station_ip, sizeof(station_ip)));
    assert(station_ip[0] == '\0');

    std::puts("Wi-Fi portal state host tests passed");
    return 0;
}
