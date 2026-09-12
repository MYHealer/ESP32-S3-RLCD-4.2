// 验证联网任务 NTP 运行态的启动、失败退避、成功和每日零点转换。
#include "network_ntp_schedule_state.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

namespace {
constexpr time_t kPlausibleNow = 1783890000;
constexpr time_t kNextMidnight = 1783900800;
}

int main()
{
    setenv("TZ", "UTC0", 1);
    tzset();

    NetworkNtpScheduleState cold =
        initialize_network_ntp_schedule(false, kPlausibleNow);
    assert(cold.boot_due);
    assert(!cold.daily_pending);
    assert(cold.next_daily_at == 0);
    assert(cold.next_retry_at == 0);
    assert(cold.retry_failures == 0);

    NetworkNtpScheduleState synced =
        initialize_network_ntp_schedule(true, kPlausibleNow);
    assert(!synced.boot_due);
    assert(synced.next_daily_at == kNextMidnight);

    NetworkNtpRetryUpdate retry = finish_network_ntp_attempt(
        &cold, false, true, false, 100);
    assert(retry.scheduled);
    assert(retry.delay_seconds == 15);
    assert(cold.next_retry_at == 115);
    assert(cold.retry_failures == 1);
    retry = finish_network_ntp_attempt(&cold, false, true, false, 200);
    assert(retry.delay_seconds == 30);
    assert(cold.next_retry_at == 230);
    assert(cold.retry_failures == 2);

    NetworkNtpScheduleState valid_time_retry = {};
    retry = finish_network_ntp_attempt(
        &valid_time_retry, false, true, true, kPlausibleNow);
    assert(retry.scheduled);
    assert(retry.delay_seconds == 5 * 60);
    assert(valid_time_retry.next_retry_at == kPlausibleNow + 5 * 60);
    assert(valid_time_retry.retry_failures == 1);

    NetworkNtpScheduleState manual_only = {};
    manual_only.next_retry_at = 999;
    manual_only.retry_failures = 4;
    retry = finish_network_ntp_attempt(
        &manual_only, false, false, true, kPlausibleNow);
    assert(!retry.scheduled);
    assert(manual_only.next_retry_at == 0);
    assert(manual_only.retry_failures == 0);

    cold.daily_pending = true;
    retry = finish_network_ntp_attempt(
        &cold, true, true, true, kPlausibleNow);
    assert(!retry.scheduled);
    assert(!cold.boot_due);
    assert(!cold.daily_pending);
    assert(cold.next_retry_at == 0);
    assert(cold.retry_failures == 0);
    assert(cold.next_daily_at == kNextMidnight);

    synced.next_daily_at = kPlausibleNow;
    refresh_network_ntp_daily_due(&synced, kPlausibleNow, true);
    assert(synced.daily_pending);
    assert(synced.next_daily_at == 0);

    synced.daily_pending = false;
    synced.boot_due = false;
    refresh_network_ntp_daily_due(&synced, kPlausibleNow, true);
    assert(!synced.daily_pending);
    assert(synced.next_daily_at == kNextMidnight);

    schedule_network_ntp_after_provisioning(&synced);
    assert(synced.boot_due);
    assert(!synced.daily_pending);
    assert(synced.next_daily_at == 0);
    assert(synced.next_retry_at == 0);
    assert(synced.retry_failures == 0);

    return 0;
}
