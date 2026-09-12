// 验证显示 DMA 原子模式状态的嵌套、饱和、下溢和并发语义。
#include "display_dma_mode_state.h"

#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

namespace {
constexpr int kThreadCount = 4;
constexpr int kIterations = 50000;
constexpr uint32_t kNestedOwnerCount = 12;
}

int main()
{
    DisplayDmaModeState state;
    assert(!state.conservative_mode());

    state.release_conservative_mode();
    assert(!state.conservative_mode());

    state.acquire_conservative_mode();
    assert(state.conservative_mode());
    state.set_ota_quiet(true);
    state.release_conservative_mode();
    assert(state.conservative_mode());
    state.set_ota_quiet(false);
    assert(!state.conservative_mode());

    static_assert(kNestedOwnerCount > 8,
                  "nested-owner regression must exceed the legacy depth limit");
    static_assert(kNestedOwnerCount <= kDisplayDmaConservativeMaxDepth,
                  "nested-owner regression must fit the production counter");
    for (uint32_t i = 0; i < kNestedOwnerCount; ++i) {
        state.acquire_conservative_mode();
    }
    assert(state.conservative_mode());
    for (uint32_t i = 0; i < kNestedOwnerCount - 1; ++i) {
        state.release_conservative_mode();
    }
    assert(state.conservative_mode());
    state.release_conservative_mode();
    assert(!state.conservative_mode());

    std::atomic<bool> invalid_state_seen{false};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (int thread = 0; thread < kThreadCount; ++thread) {
        workers.emplace_back([&] {
            for (int i = 0; i < kIterations; ++i) {
                state.acquire_conservative_mode();
                if (!state.conservative_mode()) {
                    invalid_state_seen.store(true, std::memory_order_relaxed);
                }
                state.release_conservative_mode();
            }
        });
    }
    for (std::thread &worker : workers) {
        worker.join();
    }

    assert(!invalid_state_seen.load(std::memory_order_relaxed));
    assert(!state.conservative_mode());
    return 0;
}
