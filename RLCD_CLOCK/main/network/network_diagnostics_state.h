// 提供网络检测页面请求、运行状态与逐行文本的线程安全接口。
#pragma once

#include "network_diagnostics_catalog.h"

#include <stdint.h>

enum NetworkDiagState : int {
    kNetworkDiagIdle = 0,
    kNetworkDiagRunning = 1,
    kNetworkDiagDone = 2,
};

struct NetworkDiagnosticsSnapshot {
    NetworkDiagState state = kNetworkDiagIdle;
    char lines[kNetworkDiagLineCount][kNetworkDiagLineLen] = {};
};

struct NetworkDiagPageRequestSnapshot {
    bool requested = false;
    uint32_t revision = 0;
};

NetworkDiagState network_diag_state_load();
// 读取成功才改写输出；互斥读取失败时保留调用方已有快照。
bool network_diag_snapshot_load(NetworkDiagnosticsSnapshot *snapshot);
bool network_diag_page_requested();
NetworkDiagPageRequestSnapshot network_diag_page_snapshot_load();
void network_diag_page_request();
void network_diag_page_clear();
bool network_diag_page_clear_if_current(
    const NetworkDiagPageRequestSnapshot &snapshot);

static_assert(kNetworkDiagIdle == 0,
              "network diagnostics idle state must remain the zero default");
static_assert(kNetworkDiagDone == kNetworkDiagRunning + 1,
              "network diagnostics states must remain contiguous");
