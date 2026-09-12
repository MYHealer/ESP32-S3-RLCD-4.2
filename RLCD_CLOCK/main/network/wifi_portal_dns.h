// 声明配网强制门户 DHCP/DNS 内部配置入口。
#pragma once

#include "esp_netif.h"

void configure_captive_portal_dhcp(esp_netif_t *ap_netif);
bool restart_captive_portal_dhcp(esp_netif_t *ap_netif);
bool start_captive_dns_server();
void stop_captive_dns_server();
