// 验证声音四项配置的原子快照、音量独立更新和并发一致性。
#include "chime_runtime_state.h"
#include "chime_runtime_state_internal.h"

#include "chime_settings.h"

#include <assert.h>
#include <atomic>
#include <thread>

namespace {
bool same_snapshot(const ChimeRuntimeSnapshot &left, const ChimeRuntimeSnapshot &right)
{
    return left.hourly_enabled == right.hourly_enabled &&
           left.all_day == right.all_day &&
           left.volume_percent == right.volume_percent &&
           left.sound_index == right.sound_index;
}

void test_default_and_supported_updates()
{
    ChimeRuntimeSnapshot snapshot = chime_runtime_snapshot_load();
    assert(!snapshot.hourly_enabled);
    assert(!snapshot.all_day);
    assert(snapshot.volume_percent == chime_settings::kDefaultVolumePercent);
    assert(snapshot.sound_index == 0);

    chime_runtime_snapshot_store({true, true, 40, 2});
    chime_runtime_volume_percent_store(60);
    snapshot = chime_runtime_snapshot_load();
    assert(snapshot.hourly_enabled);
    assert(snapshot.all_day);
    assert(snapshot.volume_percent == 60);
    assert(snapshot.sound_index == 2);
    assert(chime_runtime_any_enabled());
    assert(chime_runtime_volume_percent() == 60);
    assert(chime_runtime_sound_index() == 2);

    chime_runtime_snapshot_store({false, false, 60, 3});
    assert(!chime_runtime_any_enabled());
    snapshot = chime_runtime_snapshot_load();
    assert(snapshot.volume_percent == 60);
    assert(snapshot.sound_index == 3);
}

void test_concurrent_snapshot_publication()
{
    constexpr ChimeRuntimeSnapshot kFirst = {true, false, 20, 1};
    constexpr ChimeRuntimeSnapshot kSecond = {false, true, 100, 3};
    constexpr int kIterations = 100000;
    chime_runtime_snapshot_store(kFirst);
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};

    std::thread writer([&]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < kIterations; ++i) {
            chime_runtime_snapshot_store((i & 1) ? kFirst : kSecond);
        }
    });
    std::thread reader([&]() {
        start.store(true, std::memory_order_release);
        for (int i = 0; i < kIterations; ++i) {
            ChimeRuntimeSnapshot snapshot = chime_runtime_snapshot_load();
            if (!same_snapshot(snapshot, kFirst) && !same_snapshot(snapshot, kSecond)) {
                failed.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });
    writer.join();
    reader.join();
    assert(!failed.load(std::memory_order_relaxed));
}
} // namespace

int main()
{
    test_default_and_supported_updates();
    test_concurrent_snapshot_publication();
    return 0;
}
