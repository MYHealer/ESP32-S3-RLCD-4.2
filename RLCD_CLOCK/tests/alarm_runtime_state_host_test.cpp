// 验证闹钟复合快照、原子启用镜像和覆盖确认始终保持一致状态。
#include "alarm_runtime_state_internal.h"

#include <assert.h>
#include <atomic>
#include <thread>

namespace {
bool is_state_a(const AlarmSnapshot &snapshot)
{
    return !snapshot.enabled && snapshot.ringing &&
           snapshot.hour == 6 && snapshot.minute == 30;
}

bool is_state_b(const AlarmSnapshot &snapshot)
{
    return snapshot.enabled && !snapshot.ringing &&
           snapshot.hour == 7 && snapshot.minute == 45;
}
}

int main()
{
    static_assert(sizeof(AlarmSnapshot) == 4,
                  "alarm snapshot should contain only visible runtime fields");
    AlarmSnapshot snapshot = {};
    assert(!alarm_runtime_snapshot(&snapshot));
    assert(!alarm_runtime_publish(true, false, 6, 30));
    assert(!alarm_runtime_clear_replacement());
    assert(!alarm_runtime_is_enabled());

    assert(alarm_runtime_state_init());
    assert(alarm_runtime_state_init());
    assert(alarm_runtime_snapshot(&snapshot));
    assert(!snapshot.enabled && !snapshot.ringing);
    assert(snapshot.hour == 0 && snapshot.minute == 0);

    AlarmPendingSaveSnapshot pending = {};
    assert(alarm_runtime_pending_save_snapshot(&pending));
    assert(!pending.pending);
    assert(!alarm_runtime_pending_save_exists());

    assert(alarm_runtime_publish_deferred_save(true, false, 6, 30));
    assert(alarm_runtime_pending_save_snapshot(&pending));
    assert(pending.pending && pending.enabled);
    assert(pending.hour == 6 && pending.minute == 30);
    const uint32_t first_pending_generation = pending.generation;
    assert(first_pending_generation != 0);

    assert(alarm_runtime_publish_deferred_save(false, false, 6, 30));
    assert(!alarm_runtime_pending_save_clear(first_pending_generation));
    assert(alarm_runtime_pending_save_snapshot(&pending));
    assert(pending.pending && !pending.enabled);
    assert(pending.hour == 6 && pending.minute == 30);
    assert(pending.generation != first_pending_generation);
    assert(alarm_runtime_pending_save_clear(pending.generation));
    assert(!alarm_runtime_pending_save_exists());

    assert(alarm_runtime_publish_deferred_save(true, false, 8, 20));
    assert(alarm_runtime_pending_save_discard());
    assert(!alarm_runtime_pending_save_exists());

    assert(alarm_runtime_publish(true, false, 6, 30));
    assert(alarm_runtime_is_enabled());
    AlarmSnapshot existing = {};
    assert(alarm_runtime_replacement_decision(7, 45, false, 1000, 120000, &existing) ==
           kAlarmReplacementConfirmationRequired);
    assert(existing.enabled && existing.hour == 6 && existing.minute == 30);
    assert(alarm_runtime_replacement_decision(7, 45, true, 1001, 120000, &existing) ==
           kAlarmReplacementAccepted);

    assert(alarm_runtime_replacement_decision(8, 15, false, 2000, 120000, nullptr) ==
           kAlarmReplacementConfirmationRequired);
    assert(alarm_runtime_publish(true, false, 9, 5));
    assert(alarm_runtime_replacement_decision(8, 15, true, 2001, 120000, nullptr) ==
           kAlarmReplacementConfirmationInvalid);
    assert(alarm_runtime_clear_replacement());

    assert(alarm_runtime_publish(false, true, 6, 30));
    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            if (i & 1) {
                assert(alarm_runtime_publish(true, false, 7, 45));
            } else {
                assert(alarm_runtime_publish(false, true, 6, 30));
            }
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        assert(alarm_runtime_snapshot(&snapshot));
        assert(is_state_a(snapshot) || is_state_b(snapshot));
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();

    assert(alarm_runtime_snapshot(&snapshot));
    assert(is_state_b(snapshot));
    assert(alarm_runtime_is_enabled());

    assert(alarm_runtime_publish_deferred_save(false, false, 6, 30));
    std::atomic<bool> pending_writer_done{false};
    std::thread pending_writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            if (i & 1) {
                assert(alarm_runtime_publish_deferred_save(
                    true, false, 7, 45));
            } else {
                assert(alarm_runtime_publish_deferred_save(
                    false, false, 6, 30));
            }
        }
        pending_writer_done.store(true, std::memory_order_release);
    });
    do {
        assert(alarm_runtime_pending_save_snapshot(&pending));
        assert(pending.pending);
        const bool is_pending_a =
            !pending.enabled && pending.hour == 6 && pending.minute == 30;
        const bool is_pending_b =
            pending.enabled && pending.hour == 7 && pending.minute == 45;
        assert(is_pending_a || is_pending_b);
        assert(pending.generation != 0);
    } while (!pending_writer_done.load(std::memory_order_acquire));
    pending_writer.join();

    assert(alarm_runtime_pending_save_snapshot(&pending));
    assert(alarm_runtime_pending_save_clear(pending.generation));
    assert(!alarm_runtime_pending_save_exists());
    return 0;
}
