// 使用静态任务互斥发布小智天气城市待保存快照，避免在临界区复制 UTF-8。
#include "weather_city_pending_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <esp_attr.h>
#include <string.h>

namespace {
StaticTaskMutex s_pending_mutex;
EXT_RAM_BSS_ATTR char s_pending_city_text[kManualWeatherCityLen] = {};
bool s_pending = false;
uint32_t s_generation = 0;

static_assert(sizeof(s_pending_city_text) == kManualWeatherCityLen,
              "pending weather city storage must match the shared contract");

uint32_t next_generation(uint32_t current)
{
    ++current;
    return current == 0 ? 1 : current;
}
}

bool weather_city_pending_state_init()
{
    return s_pending_mutex.init();
}

bool weather_city_pending_store(const char *city)
{
    char replacement[kManualWeatherCityLen] = {};
    strlcpy(replacement, city ? city : "", sizeof(replacement));

    ScopedSemaphoreLock lock(s_pending_mutex);
    if (!lock) {
        return false;
    }
    memcpy(s_pending_city_text, replacement, sizeof(s_pending_city_text));
    s_generation = next_generation(s_generation);
    s_pending = true;
    return true;
}

bool weather_city_pending_snapshot(WeatherCityPendingSnapshot *out)
{
    if (!out) {
        return false;
    }
    *out = {};
    ScopedSemaphoreLock lock(s_pending_mutex);
    if (!lock) {
        return false;
    }
    memcpy(out->city, s_pending_city_text, sizeof(out->city));
    out->pending = s_pending;
    out->generation = s_generation;
    return true;
}

bool weather_city_pending_exists()
{
    ScopedSemaphoreLock lock(s_pending_mutex);
    return lock && s_pending;
}

bool weather_city_pending_clear(uint32_t generation)
{
    ScopedSemaphoreLock lock(s_pending_mutex);
    if (!lock || !s_pending || generation != s_generation) {
        return false;
    }
    memset(s_pending_city_text, 0, sizeof(s_pending_city_text));
    s_pending = false;
    return true;
}
