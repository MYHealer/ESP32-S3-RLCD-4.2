// 集中维护小智页面级 Wi-Fi 保活、网络 PM 锁和待唤醒省电状态。
#pragma once

struct XiaozhiPowerSessionSnapshot {
    bool network_keepalive;
    bool network_lock_held;
    bool idle_low_power;
};

bool xiaozhi_power_session_acquire_realtime();
bool xiaozhi_power_session_set_idle(bool enabled);
void xiaozhi_power_session_release();
bool xiaozhi_power_session_keepalive_active();
bool xiaozhi_power_session_task_start_blocked();
XiaozhiPowerSessionSnapshot xiaozhi_power_session_snapshot();
