// 实现可见页面天气整点和每日文字跨日缓存过期规则。
#include "ui_visible_cache.h"

#include "network_cache_policy.h"

bool ui_weather_cache_stale(time_t now_value, time_t last_sync_time)
{
    return !network_weather_cache_current_hour(now_value, last_sync_time);
}

bool ui_daily_saying_cache_stale(time_t now_value,
                                 bool snapshot_ready,
                                 time_t last_sync_time)
{
    return !snapshot_ready ||
           !network_daily_saying_cache_current_day(now_value, last_sync_time);
}
