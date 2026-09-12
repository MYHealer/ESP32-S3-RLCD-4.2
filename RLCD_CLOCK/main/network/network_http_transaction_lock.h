// 声明 HTTPS/WSS 全局事务锁的应用期取得与释放接口。
#pragma once

#include "freertos/FreeRTOS.h"

bool acquire_network_http_transaction_lock(TickType_t timeout);
void release_network_http_transaction_lock();
