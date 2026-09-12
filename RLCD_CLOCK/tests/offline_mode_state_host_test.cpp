// 验证离线模式运行态默认值、切换和并发读取保持原子可见。
#include "offline_mode_state_internal.h"

#include <assert.h>
#include <atomic>
#include <thread>

int main()
{
    assert(!offline_mode_enabled_load());
    offline_mode_enabled_store(true);
    assert(offline_mode_enabled_load());
    offline_mode_enabled_store(false);
    assert(!offline_mode_enabled_load());

    constexpr int kIterations = 100000;
    std::atomic<bool> start{false};
    std::thread writer([&]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < kIterations; ++i) {
            offline_mode_enabled_store((i & 1) != 0);
        }
    });
    std::thread reader([&]() {
        start.store(true, std::memory_order_release);
        for (int i = 0; i < kIterations; ++i) {
            (void)offline_mode_enabled_load();
        }
    });
    writer.join();
    reader.join();
    assert(offline_mode_enabled_load());
    return 0;
}
