// 验证设置页活动状态在并发发布和读取时保持动作与时间的先后关系。
#include "ui_settings_activity_state_internal.h"

#include <atomic>
#include <cassert>
#include <thread>

namespace {
constexpr uint32_t kActionCount = 100000;
}

int main()
{
    assert(settings_activity_state_init());
    assert(!settings_page_requested());
    settings_page_request();
    assert(settings_page_requested());
    settings_page_clear();
    assert(!settings_page_requested());

    assert(settings_activity_action_sequence() == 0);
    assert(settings_activity_last_tick() == 0);
    const SettingsActivitySnapshot initial = settings_activity_snapshot();
    assert(initial.action_sequence == 0);
    assert(initial.last_activity_tick == 0);
    assert(initial.revision == 0);

    settings_activity_record(17);
    assert(settings_activity_last_tick() == 17);
    const SettingsActivitySnapshot before_later_activity =
        settings_activity_snapshot();
    assert(before_later_activity.action_sequence == 0);
    assert(before_later_activity.last_activity_tick == 17);
    assert(before_later_activity.revision == 1);
    settings_activity_record(18);
    settings_page_request();
    assert(!settings_page_clear_if_activity_current(before_later_activity));
    assert(settings_page_requested());

    std::atomic<bool> invalid_order_seen{false};
    std::thread writer([] {
        for (uint32_t i = 1; i <= kActionCount; ++i) {
            settings_activity_record_action(i + 18);
        }
    });
    std::thread reader([&] {
        uint32_t observed_sequence = 0;
        while (observed_sequence < kActionCount) {
            const SettingsActivitySnapshot snapshot =
                settings_activity_snapshot();
            uint32_t sequence = snapshot.action_sequence;
            if (sequence != observed_sequence) {
                if (snapshot.last_activity_tick < sequence + 18) {
                    invalid_order_seen.store(true, std::memory_order_relaxed);
                }
                observed_sequence = sequence;
            }
        }
    });
    writer.join();
    reader.join();

    assert(!invalid_order_seen.load(std::memory_order_relaxed));
    assert(settings_activity_action_sequence() == kActionCount);
    assert(settings_activity_last_tick() == kActionCount + 18);
    const SettingsActivitySnapshot final_activity =
        settings_activity_snapshot();
    assert(final_activity.action_sequence == kActionCount);
    assert(final_activity.last_activity_tick == kActionCount + 18);
    assert(final_activity.revision == kActionCount + 2);
    assert(settings_page_clear_if_activity_current(final_activity));
    assert(!settings_page_requested());

    const SettingsActivitySnapshot before_claim =
        settings_activity_snapshot();
    assert(settings_activity_claim_if_current(before_claim));
    assert(!settings_activity_claim_if_current(before_claim));
    assert(settings_activity_snapshot().revision == before_claim.revision + 1);

    std::atomic<bool> request_test_started{false};
    std::thread request_writer([&request_test_started] {
        request_test_started.store(true, std::memory_order_release);
        for (uint32_t i = 0; i < kActionCount; ++i) {
            settings_page_request();
            settings_page_clear();
        }
    });
    std::thread request_reader([&request_test_started] {
        while (!request_test_started.load(std::memory_order_acquire)) {
        }
        for (uint32_t i = 0; i < kActionCount; ++i) {
            (void)settings_page_requested();
        }
    });
    request_writer.join();
    request_reader.join();
    assert(!settings_page_requested());
    return 0;
}
