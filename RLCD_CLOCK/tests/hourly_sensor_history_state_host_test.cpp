// 验证小时温湿度历史状态的完整快照、版本和并发发布一致性。
#include "hourly_sensor_history_state_internal.h"

#include <assert.h>
#include <atomic>
#include <string.h>
#include <thread>

namespace {
HourlySensorHistoryBlob make_history(int seed)
{
    HourlySensorHistoryBlob history = {};
    for (int i = 0; i < kHourlyHistoryCount; ++i) {
        history.samples[i].timestamp = seed * 1000 + i;
        history.samples[i].temperature = static_cast<float>(seed + i);
        history.samples[i].humidity = static_cast<float>(seed * 2 + i);
        history.samples[i].valid = 1;
    }
    return history;
}

bool history_equal(const HourlySensorHistoryBlob &lhs,
                   const HourlySensorHistoryBlob &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}
} // namespace

int main()
{
    static_assert(sizeof(HourlySensorHistoryBlob) > 1024,
                  "host test must cover a large history snapshot");
    HourlySensorHistoryBlob snapshot = make_history(99);
    uint32_t version = 99;
    assert(hourly_sensor_history_version_load() == 0);
    assert(!hourly_sensor_history_snapshot(&snapshot, &version));
    assert(snapshot.magic == kHourlyHistoryMagic);
    assert(snapshot.count == kHourlyHistoryCount);
    for (const HourlySensorSample &sample : snapshot.samples) {
        assert(!sample.valid);
    }
    assert(version == 0);
    assert(hourly_sensor_history_last_saved_at() == 0);

    assert(init_hourly_sensor_history_state());
    assert(init_hourly_sensor_history_state());
    assert(reset_hourly_sensor_history_state());
    assert(hourly_sensor_history_snapshot(&snapshot, &version));
    assert(snapshot.magic == kHourlyHistoryMagic);
    assert(snapshot.count == kHourlyHistoryCount);
    assert(version == 1);
    assert(hourly_sensor_history_version_load() == 1);

    const HourlySensorHistoryBlob history_a = make_history(10);
    const HourlySensorHistoryBlob history_b = make_history(20);
    assert(publish_loaded_hourly_sensor_history(history_a, 100));
    assert(hourly_sensor_history_snapshot(&snapshot, &version));
    assert(history_equal(snapshot, history_a));
    assert(version == 2);
    assert(hourly_sensor_history_version_load() == 2);
    assert(hourly_sensor_history_last_saved_at() == 100);

    std::atomic<bool> writer_done{false};
    uint32_t last_version = hourly_sensor_history_version_load();
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            const bool use_a = (i & 1) == 0;
            assert(publish_loaded_hourly_sensor_history(use_a ? history_a : history_b,
                                                        use_a ? 100 : 200));
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        assert(hourly_sensor_history_snapshot(&snapshot, nullptr));
        assert(history_equal(snapshot, history_a) || history_equal(snapshot, history_b));
        const uint32_t current_version = hourly_sensor_history_version_load();
        assert(current_version >= last_version);
        last_version = current_version;
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();

    assert(publish_loaded_hourly_sensor_history(history_a, 100));
    assert(hourly_sensor_history_snapshot(nullptr, &version));
    const uint32_t before_sample_version = version;
    HourlySensorSample replacement = {};
    replacement.timestamp = 9999;
    replacement.temperature = 25.5f;
    replacement.humidity = 60.5f;
    replacement.valid = 1;
    assert(!publish_hourly_sensor_sample(-1, 9999, replacement));
    assert(!publish_hourly_sensor_sample(kHourlyHistoryCount, 9999, replacement));
    assert(publish_hourly_sensor_sample(7, 9999, replacement));
    assert(hourly_sensor_history_snapshot(&snapshot, &version));
    assert(memcmp(&snapshot.samples[7], &replacement, sizeof(replacement)) == 0);
    assert(version == before_sample_version + 1);
    assert(hourly_sensor_history_version_load() == version);
    assert(hourly_sensor_history_last_saved_at() == 9999);
    assert(!hourly_sensor_history_snapshot(nullptr, nullptr));
    return 0;
}
