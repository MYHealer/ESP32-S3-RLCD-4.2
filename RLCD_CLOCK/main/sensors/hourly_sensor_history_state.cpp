// 使用静态任务互斥维护温湿度小时历史、版本和最后保存时间。
#include "hourly_sensor_history_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <esp_attr.h>

#include <atomic>

namespace {
StaticTaskMutex s_hourly_history_mutex;
EXT_RAM_BSS_ATTR HourlySensorHistoryBlob s_hourly_history;
int64_t s_last_hourly_saved_at = 0;
std::atomic<uint32_t> s_hourly_history_version{0};

void clear_history_locked()
{
    s_hourly_history = HourlySensorHistoryBlob{};
    s_last_hourly_saved_at = 0;
}
} // namespace

bool init_hourly_sensor_history_state()
{
    return s_hourly_history_mutex.init();
}

bool reset_hourly_sensor_history_state()
{
    ScopedSemaphoreLock lock(s_hourly_history_mutex.handle());
    if (!lock) {
        return false;
    }
    clear_history_locked();
    s_hourly_history_version.fetch_add(1U, std::memory_order_release);
    return true;
}

bool publish_loaded_hourly_sensor_history(const HourlySensorHistoryBlob &history,
                                          int64_t last_saved_at)
{
    ScopedSemaphoreLock lock(s_hourly_history_mutex.handle());
    if (!lock) {
        return false;
    }
    s_hourly_history = history;
    s_last_hourly_saved_at = last_saved_at;
    s_hourly_history_version.fetch_add(1U, std::memory_order_release);
    return true;
}

bool publish_hourly_sensor_sample(int index,
                                  int64_t last_saved_at,
                                  const HourlySensorSample &sample)
{
    if (index < 0 || index >= kHourlyHistoryCount) {
        return false;
    }
    ScopedSemaphoreLock lock(s_hourly_history_mutex.handle());
    if (!lock) {
        return false;
    }
    s_hourly_history.samples[index] = sample;
    s_last_hourly_saved_at = last_saved_at;
    s_hourly_history_version.fetch_add(1U, std::memory_order_release);
    return true;
}

int64_t hourly_sensor_history_last_saved_at()
{
    ScopedSemaphoreLock lock(s_hourly_history_mutex.handle());
    return lock ? s_last_hourly_saved_at : 0;
}

uint32_t hourly_sensor_history_version_load()
{
    return s_hourly_history_version.load(std::memory_order_acquire);
}

bool hourly_sensor_history_snapshot(HourlySensorHistoryBlob *history,
                                    uint32_t *version)
{
    if (!history && !version) {
        return false;
    }
    ScopedSemaphoreLock lock(s_hourly_history_mutex.handle());
    if (!lock) {
        if (history) {
            *history = {};
        }
        if (version) {
            *version = 0;
        }
        return false;
    }
    if (history) {
        *history = s_hourly_history;
    }
    if (version) {
        *version = s_hourly_history_version.load(std::memory_order_relaxed);
    }
    return true;
}
