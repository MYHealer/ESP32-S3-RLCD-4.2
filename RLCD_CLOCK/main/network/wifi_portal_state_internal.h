// 声明仅供配网子系统发布门户会话状态的内部接口。
#pragma once

#include "wifi_portal_state.h"

bool wifi_portal_state_init();
void setup_portal_active_store(bool active);
void record_wifi_disconnect_reason(int reason);
void clear_wifi_last_disconnect_reason();
void wifi_setup_ap_ssid_store(const char *ssid);
void wifi_station_ip_store(const char *ip_text);
void clear_wifi_station_ip();
WifiPortalSaveSnapshot wifi_portal_begin_save_attempt();
void wifi_portal_save_result_store(WifiPortalSaveResult result);
bool wifi_portal_save_result_store_if_generation(
    uint32_t generation,
    WifiPortalSaveResult result,
    WifiPortalSaveSnapshot *updated);
void wifi_portal_save_feedback_seen_store(bool seen);
bool wifi_portal_mark_save_feedback_seen(
    const WifiPortalSaveSnapshot &snapshot);
void wifi_portal_session_reset();
void wifi_portal_ap_channel_transition_begin();
void wifi_portal_ap_channel_transition_end();
uint8_t wifi_portal_ap_client_connected(uint8_t max_clients);
uint8_t wifi_portal_ap_client_disconnected();
bool wifi_portal_should_restart_dhcp();
