// 声明可见页面天气与每日文字缓存的新鲜度纯判断。
#pragma once

#include <stdint.h>
#include <time.h>

constexpr bool ui_visible_cache_status_refresh_due(bool cache_valid,
                                                   uint32_t cached_version,
                                                   uint32_t current_version)
{
    return !cache_valid || cached_version != current_version;
}

template <typename Snapshot>
bool ui_visible_cache_snapshot_try_refresh(
    Snapshot *cached,
    bool *cache_valid,
    bool (*snapshot_load)(Snapshot *))
{
    if (!cached || !cache_valid || !snapshot_load) {
        return false;
    }
    Snapshot candidate = *cached;
    if (!snapshot_load(&candidate)) {
        return false;
    }
    *cached = candidate;
    *cache_valid = true;
    return true;
}

bool ui_weather_cache_stale(time_t now_value, time_t last_sync_time);
bool ui_daily_saying_cache_stale(time_t now_value,
                                 bool snapshot_ready,
                                 time_t last_sync_time);
