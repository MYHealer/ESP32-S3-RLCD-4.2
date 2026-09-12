// 声明非配网联网同步在多段请求之间继续执行的运行态策略。
#pragma once

struct NetworkSyncAvailability;

struct NetworkSyncContinuationState {
    bool offline_mode = false;
    bool low_battery_mode = false;
    bool ota_blocks_background_sync = false;
    bool setup_portal_start_requested = false;
};

constexpr bool network_sync_continuation_allowed(
    const NetworkSyncContinuationState &state)
{
    return !state.offline_mode &&
           !state.low_battery_mode &&
           !state.ota_blocks_background_sync &&
           !state.setup_portal_start_requested;
}

// 只在一个 HTTP 操作已经结束、下一个操作尚未开始的边界调用；不负责
// 中断进行中的驱动或 TLS 请求。
bool network_sync_continuation_allowed();

// 在网络任务准备执行一轮同步时统一读取凭据与运行态阻断条件。
NetworkSyncAvailability capture_network_runtime_availability();

// 比较排队时与启动前的运行态，并合并 OTA、配网启动这类即时阻断条件。
bool network_sync_start_context_changed(
    const NetworkSyncAvailability &scheduled,
    const NetworkSyncAvailability &current);
