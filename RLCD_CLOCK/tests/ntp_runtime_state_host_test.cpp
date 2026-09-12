// 验证 NTP 最近成功时间在初始化边界和并发读写下保持完整值。
#include "ntp_runtime_state_internal.h"

#include <atomic>
#include <cassert>
#include <thread>

namespace {
constexpr time_t kFirstSyncTime = static_cast<time_t>(0x1234567812345678LL);
constexpr time_t kSecondSyncTime = static_cast<time_t>(0x7654321076543210LL);
constexpr int kIterations = 100000;
}

int main()
{
    assert(ntp_last_sync_time_load() == 0);
    ntp_last_sync_time_store(kFirstSyncTime);
    assert(ntp_last_sync_time_load() == 0);

    assert(ntp_runtime_state_init());
    assert(ntp_runtime_state_init());
    assert(ntp_last_sync_time_load() == 0);

    ntp_last_sync_time_store(kFirstSyncTime);
    assert(ntp_last_sync_time_load() == kFirstSyncTime);

    std::atomic<bool> invalid_snapshot_seen{false};
    std::thread writer([] {
        for (int i = 0; i < kIterations; ++i) {
            ntp_last_sync_time_store((i & 1) == 0 ? kFirstSyncTime : kSecondSyncTime);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            const time_t value = ntp_last_sync_time_load();
            if (value != kFirstSyncTime && value != kSecondSyncTime) {
                invalid_snapshot_seen.store(true, std::memory_order_relaxed);
            }
        }
    });
    writer.join();
    reader.join();

    assert(!invalid_snapshot_seen.load(std::memory_order_relaxed));
    return 0;
}
