// 声明仅供应用启动阶段使用的 Wi-Fi 驱动初始化入口。
#pragma once

#include "wifi_radio_services.h"

enum class WifiRadioIdleStopResult {
    kNoRequest,
    kDeferred,
    kStopped,
    kRetryRequired,
};

WifiRadioIdleStopResult service_wifi_radio_stop_when_idle();
void init_wifi();
