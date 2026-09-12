// 验证天气整点和每日文字跨日缓存的新鲜度与异常时间回退。
#include "ui_visible_cache.h"

#include <assert.h>
#include <stdlib.h>

namespace {
struct CacheSnapshot {
    uint32_t version = 0;
    int value = 0;
};

bool g_snapshot_load_succeeds = false;
CacheSnapshot g_snapshot_to_load = {};

bool load_cache_snapshot(CacheSnapshot *out)
{
    if (!out || !g_snapshot_load_succeeds) {
        return false;
    }
    *out = g_snapshot_to_load;
    return true;
}

struct tm local_value(int year, int month, int day, int hour, int minute, int second)
{
    struct tm value = {};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = -1;
    return value;
}

time_t local_epoch(int year, int month, int day, int hour, int minute, int second)
{
    struct tm value = local_value(year, month, day, hour, minute, second);
    return mktime(&value);
}

} // namespace

int main()
{
    assert(ui_visible_cache_status_refresh_due(false, 7, 7));
    assert(!ui_visible_cache_status_refresh_due(true, 7, 7));
    assert(ui_visible_cache_status_refresh_due(true, 7, 8));

    CacheSnapshot cache = {7, 42};
    bool cache_valid = true;
    assert(!ui_visible_cache_snapshot_try_refresh(
        &cache,
        &cache_valid,
        load_cache_snapshot));
    assert(cache.version == 7);
    assert(cache.value == 42);
    assert(cache_valid);

    g_snapshot_load_succeeds = true;
    g_snapshot_to_load = {8, 84};
    assert(ui_visible_cache_snapshot_try_refresh(
        &cache,
        &cache_valid,
        load_cache_snapshot));
    assert(cache.version == 8);
    assert(cache.value == 84);
    assert(cache_valid);
    assert(!ui_visible_cache_snapshot_try_refresh<CacheSnapshot>(
        nullptr,
        &cache_valid,
        load_cache_snapshot));
    assert(!ui_visible_cache_snapshot_try_refresh(
        &cache,
        nullptr,
        load_cache_snapshot));
    assert(!ui_visible_cache_snapshot_try_refresh<CacheSnapshot>(
        &cache,
        &cache_valid,
        nullptr));

    setenv("TZ", "Asia/Shanghai", 1);
    tzset();

    time_t weather_sync = local_epoch(2026, 7, 13, 10, 15, 0);
    assert(ui_weather_cache_stale(weather_sync, 0));
    assert(!ui_weather_cache_stale(local_epoch(2026, 7, 13, 10, 59, 59), weather_sync));
    assert(ui_weather_cache_stale(local_epoch(2026, 7, 13, 11, 0, 0), weather_sync));
    assert(ui_weather_cache_stale(local_epoch(2026, 7, 14, 10, 15, 0), weather_sync));
    assert(ui_weather_cache_stale(local_epoch(2027, 1, 1, 0, 0, 0),
                                  local_epoch(2026, 12, 31, 23, 59, 59)));

    assert(!ui_weather_cache_stale(3600, 1));
    assert(ui_weather_cache_stale(3601, 1));

    time_t saying_sync = local_epoch(2026, 7, 13, 0, 1, 0);
    assert(ui_daily_saying_cache_stale(saying_sync, false, saying_sync));
    assert(ui_daily_saying_cache_stale(saying_sync, true, 0));
    assert(!ui_daily_saying_cache_stale(local_epoch(2026, 7, 13, 23, 59, 59),
                                        true,
                                        saying_sync));
    assert(ui_daily_saying_cache_stale(local_epoch(2026, 7, 14, 0, 0, 0),
                                       true,
                                       saying_sync));
    assert(ui_daily_saying_cache_stale(local_epoch(2027, 1, 1, 0, 0, 0),
                                       true,
                                       local_epoch(2026, 12, 31, 23, 59, 59)));

    assert(!ui_daily_saying_cache_stale(86400, true, 1));
    assert(ui_daily_saying_cache_stale(86401, true, 1));
    return 0;
}
