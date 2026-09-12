// 限定 OTA 服务所有者发布运行态、下载进度和重启静默窗口。
#pragma once

#include "ota_runtime_state.h"

bool ota_runtime_state_init();
void ota_runtime_publish_status(int state,
                                const char *status,
                                int progress,
                                TickType_t status_until_tick,
                                bool status_hold_set);
void ota_runtime_publish_download_status(const char *status,
                                         int progress,
                                         int speed_kbps);
void ota_runtime_reboot_pending_store(bool pending);
void ota_runtime_reset_status_if_idle(TickType_t now, const char *idle_status);
