// 验证番茄钟复合快照、单调剩余时间和并发发布始终保持同一批次。
#include "pomodoro_runtime_state_internal.h"

#include <assert.h>
#include <atomic>
#include <thread>

namespace {
bool is_state_a(const PomodoroRuntimeSnapshot &snapshot)
{
    return snapshot.visible.state == kPomodoroRunning &&
           snapshot.visible.total_ms == 2000 &&
           snapshot.visible.remaining_ms == 2000 &&
           !snapshot.visible.alerting &&
           snapshot.completed_at_us == 0;
}

bool is_state_b(const PomodoroRuntimeSnapshot &snapshot)
{
    return snapshot.visible.state == kPomodoroCompleted &&
           snapshot.visible.total_ms == 3000 &&
           snapshot.visible.remaining_ms == 0 &&
           snapshot.visible.alerting &&
           snapshot.completed_at_us == 900000;
}
}

int main()
{
    PomodoroRuntimeSnapshot snapshot = {};
    assert(!pomodoro_runtime_snapshot(0, &snapshot));
    assert(!pomodoro_runtime_publish(kPomodoroIdle, 0, 0, false, 0, 0));
    assert(!pomodoro_runtime_set_alerting(true));
    assert(pomodoro_runtime_state_load() == kPomodoroIdle);

    assert(pomodoro_runtime_state_init());
    assert(pomodoro_runtime_state_init());
    assert(pomodoro_runtime_snapshot(0, &snapshot));
    assert(snapshot.visible.state == kPomodoroIdle);
    assert(snapshot.visible.version == 1);
    assert(pomodoro_runtime_state_load() == snapshot.visible.state);

    assert(pomodoro_runtime_publish(kPomodoroRunning,
                                    65000,
                                    65000,
                                    false,
                                    66000000,
                                    0));
    assert(pomodoro_runtime_snapshot(1000000, &snapshot));
    const uint32_t running_version = snapshot.visible.version;
    assert(snapshot.visible.remaining_ms == 65000);
    assert(pomodoro_runtime_state_load() == kPomodoroRunning);
    assert(pomodoro_runtime_snapshot(1000001, &snapshot));
    assert(snapshot.visible.remaining_ms == 65000);
    assert(snapshot.visible.version == running_version);
    assert(pomodoro_runtime_snapshot(65999999, &snapshot));
    assert(snapshot.visible.remaining_ms == 1);
    assert(pomodoro_runtime_snapshot(66000000, &snapshot));
    assert(snapshot.visible.remaining_ms == 0);

    assert(pomodoro_runtime_set_alerting(true));
    assert(pomodoro_runtime_snapshot(66000000, &snapshot));
    const uint32_t alerting_version = snapshot.visible.version;
    assert(snapshot.visible.alerting);
    assert(pomodoro_runtime_set_alerting(true));
    assert(pomodoro_runtime_snapshot(66000000, &snapshot));
    assert(snapshot.visible.version == alerting_version);

    assert(pomodoro_runtime_publish(kPomodoroRunning,
                                    2000,
                                    2000,
                                    false,
                                    3000000,
                                    0));
    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            if (i & 1) {
                assert(pomodoro_runtime_publish(kPomodoroRunning,
                                                2000,
                                                2000,
                                                false,
                                                3000000,
                                                0));
            } else {
                assert(pomodoro_runtime_publish(kPomodoroCompleted,
                                                3000,
                                                0,
                                                true,
                                                0,
                                                900000));
            }
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        const PomodoroState state = pomodoro_runtime_state_load();
        assert(state == kPomodoroRunning || state == kPomodoroCompleted);
        assert(pomodoro_runtime_snapshot(1000000, &snapshot));
        assert(is_state_a(snapshot) || is_state_b(snapshot));
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();
    assert(pomodoro_runtime_snapshot(1000000, &snapshot));
    assert(is_state_a(snapshot));
    assert(pomodoro_runtime_state_load() == snapshot.visible.state);
    return 0;
}
