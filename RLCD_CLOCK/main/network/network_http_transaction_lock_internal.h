// 限定 HTTPS/WSS 全局事务锁只能由应用启动入口初始化。
#pragma once

#include "network_http_transaction_lock.h"

bool init_network_http_transaction_lock();
