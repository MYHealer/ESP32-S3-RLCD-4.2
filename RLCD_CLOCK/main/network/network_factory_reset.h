// 声明普通用户配置的恢复出厂 NVS 批量擦除入口。
#pragma once

#include "esp_err.h"
#include "nvs.h"

namespace network_factory_reset {

esp_err_t erase_saved_config_keys(nvs_handle_t nvs);

} // namespace network_factory_reset
