// 定义显示刷新诊断窗口和日志抑制的纯判断规则。
#pragma once

#include <stdint.h>

struct DisplayFlushDiagDecision {
    bool close_window;
    bool emit_log;
};

constexpr DisplayFlushDiagDecision display_flush_diag_decision(
    bool first_window,
    bool interval_elapsed,
    bool page_changed,
    uint32_t full_refresh_cycles,
    bool ota_updating,
    bool reboot_pending)
{
    const bool close_window = first_window || interval_elapsed || page_changed;
    return {
        close_window,
        close_window && full_refresh_cycles > 0 && !ota_updating && !reboot_pending,
    };
}
