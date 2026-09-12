// 限定配置保存与启动加载模块发布联网凭据，普通消费者仅使用只读快照。
#pragma once

#include "network_credentials_state.h"

bool network_credentials_state_init();
void network_credentials_store(const char *ssid_a,
                               const char *password_a,
                               const char *ssid_b,
                               const char *password_b,
                               WifiCredentialSlot preferred_slot,
                               const char *weather_api_key,
                               const char *weather_api_host,
                               bool weather_api_key_configured,
                               bool weather_api_host_configured);
void network_credentials_clear();
bool network_wifi_credentials_for_slot_copy(WifiCredentialSlot slot,
                                            char *ssid,
                                            size_t ssid_len,
                                            char *password,
                                            size_t password_len);
bool network_wifi_select_slot(WifiCredentialSlot slot);
WifiCredentialSlot network_wifi_current_slot();
WifiCredentialSlot network_wifi_preferred_slot();
bool network_wifi_alternate_slot_configured();
void network_wifi_preferred_slot_store(WifiCredentialSlot slot);
