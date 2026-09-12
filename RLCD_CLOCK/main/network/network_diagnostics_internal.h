// 声明网络调度内部使用的检测会话和检测执行入口。
#pragma once

#include "network_diagnostics.h"

#include <stdint.h>

void network_diag_begin();
void network_diag_finish();
void network_diag_finish_with_status(const char *status_text);
void network_diag_finish_unavailable(const char *ip_location_text);

// 只执行检测项目；调用方负责 begin/finish 与 Wi-Fi 会话生命周期。
// 运行态阻断或请求代次被替换时返回 false，由调用方保留新请求并关闭
// 本轮射频；只有启动时拥有的代次可以继续写入检测状态。
bool run_network_diagnostic_checks(uint32_t request_generation);
