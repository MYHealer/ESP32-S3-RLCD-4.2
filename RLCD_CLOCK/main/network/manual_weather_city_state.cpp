// 使用静态任务互斥保存手动天气城市，避免跨任务读取半写入 UTF-8。
#include "manual_weather_city_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <esp_attr.h>
#include <string.h>

namespace {
StaticTaskMutex s_manual_weather_city_mutex;
EXT_RAM_BSS_ATTR char s_manual_weather_city_text[kManualWeatherCityLen] = {};

static_assert(sizeof(s_manual_weather_city_text) == kManualWeatherCityLen,
              "manual weather city storage must match the shared contract");
}

bool init_manual_weather_city_state()
{
    return s_manual_weather_city_mutex.init();
}

bool manual_weather_city_snapshot(char *out, size_t out_len)
{
    if (!out || out_len < sizeof(s_manual_weather_city_text)) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return false;
    }
    ScopedSemaphoreLock lock(s_manual_weather_city_mutex);
    if (!lock) {
        out[0] = '\0';
        return false;
    }
    memcpy(out, s_manual_weather_city_text, sizeof(s_manual_weather_city_text));
    return out[0] != '\0';
}

void manual_weather_city_store(const char *city)
{
    char replacement[kManualWeatherCityLen] = {};
    strlcpy(replacement, city ? city : "", sizeof(replacement));
    ScopedSemaphoreLock lock(s_manual_weather_city_mutex);
    if (!lock) {
        return;
    }
    memcpy(s_manual_weather_city_text,
           replacement,
           sizeof(s_manual_weather_city_text));
}

bool manual_weather_city_is_configured()
{
    ScopedSemaphoreLock lock(s_manual_weather_city_mutex);
    return lock && s_manual_weather_city_text[0] != '\0';
}
