// 使用静态任务互斥发布每日文字和同步时间，避免字符串复制关闭中断。
#include "daily_saying_state_internal.h"

#include "app_text_format.h"
#include "daily_saying_contract.h"
#include "scoped_semaphore_lock.h"

#include <esp_attr.h>

#include <atomic>
#include <string.h>

namespace {
StaticTaskMutex s_daily_saying_mutex;
EXT_RAM_BSS_ATTR char s_daily_saying[kDailySayingLen] = {};
time_t s_last_saying_sync_time = 0;
std::atomic<uint32_t> s_daily_saying_version{0};

void prepare_daily_saying(char (&out)[kDailySayingLen], const char *text)
{
    const char *source = text ? text : "";
    const size_t length = strnlen(source, sizeof(out) - 1);
    memmove(out, source, length);
    out[length] = '\0';
}

void copy_daily_saying_snapshot(char *out,
                                size_t out_len,
                                const char (&source)[kDailySayingLen])
{
    const size_t source_len = strnlen(source, sizeof(source));
    const size_t copy_len = source_len < out_len - 1 ? source_len : out_len - 1;
    memcpy(out, source, copy_len);
    out[copy_len] = '\0';
}

DailySayingCacheSnapshot daily_saying_cache_snapshot_locked()
{
    return {
        s_daily_saying[0] != '\0',
        s_last_saying_sync_time,
        s_daily_saying_version.load(std::memory_order_relaxed),
    };
}
} // namespace

bool daily_saying_state_init()
{
    return s_daily_saying_mutex.init();
}

void reset_daily_saying_cache()
{
    (void)daily_saying_state_publish("", 0);
}

uint32_t daily_saying_state_version_load()
{
    return s_daily_saying_version.load(std::memory_order_acquire);
}

bool daily_saying_cache_snapshot_load(DailySayingCacheSnapshot *out)
{
    if (!out) {
        return false;
    }
    *out = {};
    ScopedSemaphoreLock lock(s_daily_saying_mutex);
    if (!lock) {
        return false;
    }
    *out = daily_saying_cache_snapshot_locked();
    return true;
}

bool daily_saying_text_snapshot_load(char *out,
                                     size_t out_len,
                                     DailySayingCacheSnapshot *metadata)
{
    if (metadata) {
        *metadata = {};
    }
    if (!app_text::output_buffer_available(out, out_len)) {
        return false;
    }
    out[0] = '\0';

    ScopedSemaphoreLock lock(s_daily_saying_mutex);
    if (!lock) {
        return false;
    }

    copy_daily_saying_snapshot(out, out_len, s_daily_saying);
    if (metadata) {
        *metadata = daily_saying_cache_snapshot_locked();
    }
    return true;
}

bool daily_saying_state_publish(const char *text, time_t synced_at)
{
    ScopedSemaphoreLock lock(s_daily_saying_mutex);
    if (!lock) {
        return false;
    }
    prepare_daily_saying(s_daily_saying, text);
    s_last_saying_sync_time = synced_at;
    s_daily_saying_version.store(
        s_daily_saying_version.load(std::memory_order_relaxed) + 1U,
        std::memory_order_release);
    return true;
}
