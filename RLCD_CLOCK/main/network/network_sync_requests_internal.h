// 声明仅供常驻网络任务使用的同步请求结果结算接口。
#pragma once

#include "network_sync_requests.h"

void finish_settings_sync_and_clear_bit(SettingsSyncOp op,
                                        const char *status,
                                        EventBits_t bit,
                                        uint32_t request_generation,
                                        uint32_t settings_generation);
void finish_offline_network_requests(const NetworkSyncRequestSnapshot &requests);
void finish_unconfigured_network_requests(const NetworkSyncRequestSnapshot &requests);
void finish_low_battery_network_requests(const NetworkSyncRequestSnapshot &requests);
void finish_failed_sync_requests(const NetworkSyncRequestSnapshot &requests);
void finish_successful_sync_requests(const NetworkSyncRequestSnapshot &requests,
                                     bool ntp_ok,
                                     bool weather_ok,
                                     bool saying_ok);
void finish_network_diagnostics_sync(
    const NetworkSyncRequestSnapshot &requests);
