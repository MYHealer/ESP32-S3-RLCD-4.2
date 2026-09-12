// 使用静态任务互斥维护 NTP 最近成功时间，避免跨任务读取撕裂。
#include "ntp_runtime_state_internal.h"

#include "scoped_semaphore_lock.h"

namespace {
StaticTaskMutex s_ntp_state_mutex;
time_t s_last_ntp_sync_time = 0;
}

bool ntp_runtime_state_init()
{
    return s_ntp_state_mutex.init();
}

time_t ntp_last_sync_time_load()
{
    ScopedSemaphoreLock lock(s_ntp_state_mutex);
    return lock ? s_last_ntp_sync_time : 0;
}

void ntp_last_sync_time_store(time_t sync_time)
{
    ScopedSemaphoreLock lock(s_ntp_state_mutex);
    if (!lock) {
        return;
    }
    s_last_ntp_sync_time = sync_time;
}
