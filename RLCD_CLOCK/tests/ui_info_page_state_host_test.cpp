// 验证关于本机页面请求与截止 Tick 在并发读写下保持一致快照。
#include "ui_info_page_state_internal.h"

#include <atomic>
#include <cassert>
#include <thread>

std::atomic<bool> g_fail_mutex_take{false};

namespace {
constexpr uint32_t kFirstDeadline = 300;
constexpr uint32_t kSecondDeadline = 600;
constexpr int kIterations = 100000;
}

int main()
{
    InfoPageStateSnapshot state = {true, 99, 42};
    assert(!info_page_state_load(&state));
    assert(state.requested);
    assert(state.hold_until_tick == 99);
    assert(state.revision == 42);
    info_page_request(kFirstDeadline);
    assert(!info_page_requested());
    assert(info_page_state_init());
    assert(info_page_state_init());

    assert(info_page_state_load(&state));
    assert(!state.requested);
    assert(state.hold_until_tick == 0);
    assert(state.revision == 0);
    assert(!info_page_requested());

    info_page_hold_until_store(77);
    assert(info_page_state_load(&state));
    assert(!state.requested);
    assert(state.hold_until_tick == 77);
    const uint32_t idle_hold_revision = state.revision;
    assert(idle_hold_revision != 0);
    assert(!info_page_requested());

    info_page_request(kFirstDeadline);
    assert(info_page_requested());
    assert(info_page_state_load(&state));
    assert(state.hold_until_tick == kFirstDeadline);
    assert(state.revision != idle_hold_revision);
    const InfoPageStateSnapshot stale_deadline = state;

    InfoPageStateSnapshot preserved = state;
    g_fail_mutex_take.store(true, std::memory_order_release);
    assert(!info_page_state_load(&preserved));
    g_fail_mutex_take.store(false, std::memory_order_release);
    assert(preserved.requested);
    assert(preserved.hold_until_tick == stale_deadline.hold_until_tick);
    assert(preserved.revision == stale_deadline.revision);

    info_page_hold_until_store(kSecondDeadline);
    assert(info_page_state_load(&state));
    assert(state.requested);
    assert(state.hold_until_tick == kSecondDeadline);
    assert(state.revision != stale_deadline.revision);
    assert(!info_page_clear_if_current(stale_deadline));
    assert(info_page_requested());
    assert(info_page_clear_if_current(state));
    assert(!info_page_requested());
    assert(!info_page_clear_if_current(state));

    info_page_request(kFirstDeadline);
    assert(info_page_state_load(&state));
    const InfoPageStateSnapshot stale_request = state;
    info_page_request(kSecondDeadline);
    assert(!info_page_clear_if_current(stale_request));
    assert(info_page_requested());

    std::atomic<bool> invalid_snapshot_seen{false};
    std::thread writer([] {
        for (int i = 0; i < kIterations; ++i) {
            info_page_request((i & 1) == 0 ? kFirstDeadline : kSecondDeadline);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            InfoPageStateSnapshot snapshot;
            assert(info_page_state_load(&snapshot));
            if (!snapshot.requested ||
                (snapshot.hold_until_tick != kFirstDeadline &&
                 snapshot.hold_until_tick != kSecondDeadline)) {
                invalid_snapshot_seen.store(true, std::memory_order_relaxed);
            }
        }
    });
    writer.join();
    reader.join();

    assert(!invalid_snapshot_seen.load(std::memory_order_relaxed));
    info_page_clear();
    assert(!info_page_requested());
    assert(info_page_state_load(&state));
    assert(!state.requested);
    assert(state.hold_until_tick == 0);
    assert(state.revision != 0);
    return 0;
}
