// 验证每日文字与同步时间在并发读写中始终来自同一发布批次。
#include "daily_saying_state_internal.h"

#include "daily_saying_contract.h"

#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>

bool g_fail_daily_saying_mutex_take = false;

int main()
{
    assert(daily_saying_state_version_load() == 0);
    assert(daily_saying_state_init());
    assert(daily_saying_state_init());
    reset_daily_saying_cache();
    assert(daily_saying_state_version_load() == 1);

    char text[kDailySayingLen] = {};
    DailySayingCacheSnapshot text_snapshot = {true, 99, 99};
    assert(daily_saying_text_snapshot_load(text,
                                           sizeof(text),
                                           &text_snapshot));
    assert(text[0] == '\0');
    assert(!text_snapshot.available);
    assert(text_snapshot.last_sync_time == 0);
    assert(text_snapshot.version == 1);
    assert(!daily_saying_text_snapshot_load(nullptr,
                                            sizeof(text),
                                            &text_snapshot));
    assert(!daily_saying_text_snapshot_load(text, 0, &text_snapshot));
    DailySayingCacheSnapshot cache = {true, 99, 99};
    assert(daily_saying_cache_snapshot_load(&cache));
    assert(!cache.available);
    assert(cache.last_sync_time == 0);
    assert(cache.version == 1);
    assert(!daily_saying_cache_snapshot_load(nullptr));

    assert(daily_saying_state_publish("今日宜保持耐心", 1234));
    assert(daily_saying_state_version_load() == 2);
    assert(daily_saying_text_snapshot_load(text,
                                           sizeof(text),
                                           &text_snapshot));
    assert(strcmp(text, "今日宜保持耐心") == 0);
    assert(text_snapshot.available);
    assert(text_snapshot.last_sync_time == 1234);
    assert(text_snapshot.version == 2);
    assert(daily_saying_cache_snapshot_load(&cache));
    assert(cache.available);
    assert(cache.last_sync_time == 1234);
    assert(cache.version == 2);

    char short_text[5] = {};
    assert(daily_saying_text_snapshot_load(short_text,
                                           sizeof(short_text),
                                           nullptr));
    assert(strlen(short_text) == sizeof(short_text) - 1);

    char oversized_output[kDailySayingLen + 64] = {};
    assert(daily_saying_text_snapshot_load(oversized_output,
                                           sizeof(oversized_output),
                                           nullptr));
    assert(strcmp(oversized_output, "今日宜保持耐心") == 0);

    char long_text[kDailySayingLen + 32];
    memset(long_text, 'x', sizeof(long_text));
    long_text[sizeof(long_text) - 1] = '\0';
    assert(daily_saying_state_publish(long_text, 5678));
    assert(daily_saying_state_version_load() == 3);
    assert(daily_saying_text_snapshot_load(text,
                                           sizeof(text),
                                           &text_snapshot));
    assert(strlen(text) == kDailySayingLen - 1);
    assert(text_snapshot.last_sync_time == 5678);

    memset(text, 'x', sizeof(text));
    text_snapshot = {true, 99, 99};
    g_fail_daily_saying_mutex_take = true;
    assert(!daily_saying_text_snapshot_load(text,
                                            sizeof(text),
                                            &text_snapshot));
    g_fail_daily_saying_mutex_take = false;
    assert(text[0] == '\0');
    assert(!text_snapshot.available);
    assert(text_snapshot.last_sync_time == 0);
    assert(text_snapshot.version == 0);

    constexpr const char *kTextA = "A-每日文字批次";
    constexpr const char *kTextB = "B-每日文字批次";
    constexpr time_t kTimeA = 101;
    constexpr time_t kTimeB = 202;
    assert(daily_saying_state_publish(kTextA, kTimeA));
    uint32_t last_version = daily_saying_state_version_load();
    std::atomic<bool> writer_done{false};
    std::thread writer([&writer_done]() {
        for (int iteration = 0; iteration < 10000; ++iteration) {
            const bool use_a = (iteration & 1) == 0;
            assert(daily_saying_state_publish(use_a ? kTextA : kTextB,
                                              use_a ? kTimeA : kTimeB));
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        assert(daily_saying_text_snapshot_load(text,
                                               sizeof(text),
                                               &text_snapshot));
        const bool is_a =
            strcmp(text, kTextA) == 0 &&
            text_snapshot.last_sync_time == kTimeA;
        const bool is_b =
            strcmp(text, kTextB) == 0 &&
            text_snapshot.last_sync_time == kTimeB;
        assert(is_a || is_b);
        assert(daily_saying_cache_snapshot_load(&cache));
        assert(cache.available);
        assert(cache.last_sync_time == kTimeA || cache.last_sync_time == kTimeB);
        assert(cache.version >= last_version);
        const uint32_t current_version = daily_saying_state_version_load();
        assert(cache.version <= current_version);
        assert(current_version >= last_version);
        last_version = current_version;
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();

    assert(daily_saying_state_publish(nullptr, 0));
    assert(daily_saying_state_version_load() > last_version);
    assert(daily_saying_text_snapshot_load(text,
                                           sizeof(text),
                                           &text_snapshot));
    assert(text[0] == '\0');
    assert(!text_snapshot.available);
    assert(text_snapshot.last_sync_time == 0);
    assert(daily_saying_cache_snapshot_load(&cache));
    assert(!cache.available);
    assert(cache.last_sync_time == 0);
    assert(cache.version == daily_saying_state_version_load());
    return 0;
}
