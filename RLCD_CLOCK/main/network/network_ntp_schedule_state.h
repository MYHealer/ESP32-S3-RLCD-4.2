// 声明联网任务私有的 NTP 启动、每日零点和失败退避运行态。
#pragma once

#include <stdint.h>
#include <time.h>

struct NetworkNtpScheduleState {
    time_t next_retry_at = 0;
    time_t next_daily_at = 0;
    uint8_t retry_failures = 0;
    bool boot_due = false;
    bool daily_pending = false;
};

struct NetworkNtpRetryUpdate {
    time_t delay_seconds = 0;
    bool scheduled = false;
};

NetworkNtpScheduleState initialize_network_ntp_schedule(bool already_synced,
                                                        time_t now);
NetworkNtpRetryUpdate finish_network_ntp_attempt(
    NetworkNtpScheduleState *state,
    bool succeeded,
    bool retry_required,
    bool time_plausible,
    time_t now);
void refresh_network_ntp_daily_due(NetworkNtpScheduleState *state,
                                   time_t now,
                                   bool time_plausible);
void schedule_network_ntp_after_provisioning(
    NetworkNtpScheduleState *state);
