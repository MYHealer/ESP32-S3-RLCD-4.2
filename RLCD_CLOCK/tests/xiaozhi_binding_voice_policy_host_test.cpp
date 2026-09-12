// 验证小智绑定码重复播报、固定槽交接和并发完整性。
#include "xiaozhi_binding_voice.h"
#include "xiaozhi_binding_voice_state.h"

#include <cassert>
#include <atomic>
#include <cstring>
#include <thread>

int main()
{
    assert(!xiaozhi_binding_voice::should_announce(nullptr, ""));
    assert(!xiaozhi_binding_voice::should_announce("", ""));
    assert(!xiaozhi_binding_voice::should_announce("123456", "123456"));
    assert(xiaozhi_binding_voice::should_announce("123456", ""));
    assert(xiaozhi_binding_voice::should_announce("123456", nullptr));
    assert(xiaozhi_binding_voice::should_announce("654321", "123456"));

    for (int index = 0; index < 10; ++index) {
        assert(xiaozhi_binding_voice::digit_index(static_cast<char>('0' + index)) == index);
    }
    assert(xiaozhi_binding_voice::digit_index('/') == -1);
    assert(xiaozhi_binding_voice::digit_index(':') == -1);
    assert(xiaozhi_binding_voice::digit_index('A') == -1);

    char pending[kXiaozhiBindingCodeStorageSize] = {'x'};
    assert(!xiaozhi_binding_voice_needs_announcement("123456"));
    assert(!xiaozhi_binding_voice_record_announced("123456"));
    assert(!xiaozhi_binding_voice_store_pending("123456"));
    assert(!xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
    assert(pending[0] == '\0');

    assert(xiaozhi_binding_voice_state_init());
    assert(xiaozhi_binding_voice_state_init());
    assert(!xiaozhi_binding_voice_needs_announcement(nullptr));
    assert(!xiaozhi_binding_voice_needs_announcement(""));
    assert(xiaozhi_binding_voice_needs_announcement("123456"));
    assert(xiaozhi_binding_voice_record_announced("123456"));
    assert(!xiaozhi_binding_voice_needs_announcement("123456"));
    assert(xiaozhi_binding_voice_needs_announcement("654321"));

    assert(xiaozhi_binding_voice_store_pending("654321"));
    assert(xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
    assert(std::strcmp(pending, "654321") == 0);
    assert(!xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
    assert(pending[0] == '\0');

    constexpr const char *kCodeA = "11111111111111111111111";
    constexpr const char *kCodeB = "99999999999999999999999";
    std::atomic<int> ready_iteration{-1};
    std::atomic<int> consumed_iteration{-1};
    std::thread writer([&]() {
        for (int iteration = 0; iteration < 10000; ++iteration) {
            while (consumed_iteration.load(std::memory_order_acquire) !=
                   iteration - 1) {
                std::this_thread::yield();
            }
            assert(xiaozhi_binding_voice_store_pending(
                (iteration & 1) ? kCodeA : kCodeB));
            ready_iteration.store(iteration, std::memory_order_release);
        }
    });
    for (int iteration = 0; iteration < 10000; ++iteration) {
        while (ready_iteration.load(std::memory_order_acquire) != iteration) {
            std::this_thread::yield();
        }
        assert(xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
        assert(std::strcmp(pending, (iteration & 1) ? kCodeA : kCodeB) == 0);
        consumed_iteration.store(iteration, std::memory_order_release);
    }
    writer.join();

    assert(xiaozhi_binding_voice_record_announced("123456"));
    assert(xiaozhi_binding_voice_store_pending("654321"));
    xiaozhi_binding_voice_state_deinit();
    assert(!xiaozhi_binding_voice_needs_announcement("123456"));
    assert(!xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
    assert(pending[0] == '\0');
    assert(xiaozhi_binding_voice_state_init());
    assert(xiaozhi_binding_voice_needs_announcement("123456"));
    assert(!xiaozhi_binding_voice_take_pending(pending, sizeof(pending)));
    xiaozhi_binding_voice_state_deinit();
    return 0;
}
