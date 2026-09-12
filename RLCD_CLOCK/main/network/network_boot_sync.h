// 声明启动联网预算、同步流程和任务入口。
#pragma once

inline constexpr int kBootWifiConnectTimeoutMs = 5000;
inline constexpr int kBootNtpRetries = 2;
inline constexpr int kBootStartupBudgetMs = 6000;

int boot_sync_remaining_ms();
void run_boot_connectivity_sync();
void boot_connectivity_task(void *);
