// 验证本地温湿度当前值、趋势和版本在并发读写中保持同一批次。
#include "local_sensor_state_internal.h"

#include <assert.h>
#include <atomic>
#include <thread>

namespace {
bool snapshot_matches(float temperature,
                      float humidity,
                      int temperature_trend,
                      int humidity_trend,
                      float expected_temperature,
                      float expected_humidity,
                      int expected_temperature_trend,
                      int expected_humidity_trend)
{
    return temperature == expected_temperature &&
           humidity == expected_humidity &&
           temperature_trend == expected_temperature_trend &&
           humidity_trend == expected_humidity_trend;
}
} // namespace

int main()
{
    bool sample_changed = true;
    assert(!local_sensor_state_publish_sample(20.0f, 50.0f, 0, 0, &sample_changed));
    assert(!sample_changed);

    float unavailable_temperature = 99.0f;
    float unavailable_humidity = 88.0f;
    int unavailable_temperature_trend = 1;
    int unavailable_humidity_trend = -1;
    assert(!get_local_sensor_snapshot(&unavailable_temperature,
                                      &unavailable_humidity,
                                      &unavailable_temperature_trend,
                                      &unavailable_humidity_trend));
    assert(unavailable_temperature == 0.0f);
    assert(unavailable_humidity == 0.0f);
    assert(unavailable_temperature_trend == 0);
    assert(unavailable_humidity_trend == 0);

    LocalSensorStateSnapshot snapshot = {
        99.0f,
        88.0f,
        1,
        -1,
        42,
        true,
    };
    assert(!local_sensor_state_snapshot_load(&snapshot));
    assert(snapshot.temperature == 0.0f);
    assert(snapshot.humidity == 0.0f);
    assert(snapshot.temperature_trend == 0);
    assert(snapshot.humidity_trend == 0);
    assert(snapshot.version == 0);
    assert(!snapshot.available);

    assert(init_local_sensor_state());
    assert(init_local_sensor_state());

    float temperature = -1.0f;
    float humidity = -1.0f;
    int temperature_trend = 9;
    int humidity_trend = 9;
    assert(!get_local_sensor_snapshot(&temperature,
                                      &humidity,
                                      &temperature_trend,
                                      &humidity_trend));
    assert(temperature == 0.0f);
    assert(humidity == 0.0f);
    assert(temperature_trend == 0);
    assert(humidity_trend == 0);
    assert(local_sensor_state_version() == 0);
    assert(local_sensor_state_snapshot_load(&snapshot));
    assert(!snapshot.available);
    assert(snapshot.version == 0);
    bool unavailable_changed = true;
    assert(local_sensor_state_publish_unavailable(&unavailable_changed));
    assert(!unavailable_changed);
    assert(local_sensor_state_version() == 0);

    assert(local_sensor_state_publish_sample(21.0f, 51.0f, 1, -1, &sample_changed));
    assert(sample_changed);
    assert(get_local_sensor_snapshot(&temperature,
                                     &humidity,
                                     &temperature_trend,
                                     &humidity_trend));
    assert(snapshot_matches(temperature, humidity,
                            temperature_trend, humidity_trend,
                            21.0f, 51.0f, 1, -1));
    assert(local_sensor_state_version() == 1);
    assert(local_sensor_state_snapshot_load(&snapshot));
    assert(snapshot.available);
    assert(snapshot_matches(snapshot.temperature,
                            snapshot.humidity,
                            snapshot.temperature_trend,
                            snapshot.humidity_trend,
                            21.0f,
                            51.0f,
                            1,
                            -1));
    assert(snapshot.version == 1);
    assert(!local_sensor_state_snapshot_load(nullptr));
    sample_changed = true;
    assert(local_sensor_state_publish_sample(21.0f, 51.0f, 1, -1, &sample_changed));
    assert(!sample_changed);
    assert(local_sensor_state_version() == 1);

    constexpr int kIterations = 10000;
    assert(local_sensor_state_publish_sample(11.0f, 33.0f, 1, -1, &sample_changed));
    assert(sample_changed);
    std::atomic<bool> writer_done{false};
    std::thread writer([&writer_done]() {
        for (int iteration = 0; iteration < kIterations; ++iteration) {
            const bool use_a = (iteration & 1) != 0;
            assert(local_sensor_state_publish_sample(use_a ? 11.0f : 22.0f,
                                                     use_a ? 33.0f : 44.0f,
                                                     use_a ? 1 : -1,
                                                     use_a ? -1 : 1));
        }
        writer_done.store(true, std::memory_order_release);
    });
    uint32_t observed_version = local_sensor_state_version();
    do {
        const uint32_t current_version = local_sensor_state_version();
        assert(current_version >= observed_version);
        observed_version = current_version;
        assert(get_local_sensor_snapshot(&temperature,
                                         &humidity,
                                         &temperature_trend,
                                         &humidity_trend));
        const bool is_a = snapshot_matches(temperature, humidity,
                                           temperature_trend, humidity_trend,
                                           11.0f, 33.0f, 1, -1);
        const bool is_b = snapshot_matches(temperature, humidity,
                                           temperature_trend, humidity_trend,
                                           22.0f, 44.0f, -1, 1);
        assert(is_a || is_b);
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();
    assert(local_sensor_state_version() == static_cast<uint32_t>(kIterations + 2));

    const uint32_t version_before_failure = local_sensor_state_version();
    unavailable_changed = false;
    assert(local_sensor_state_publish_unavailable(&unavailable_changed));
    assert(unavailable_changed);
    assert(local_sensor_state_snapshot_load(&snapshot));
    assert(!snapshot.available);
    assert(snapshot.version == version_before_failure + 1);
    assert(!get_local_sensor_snapshot(&temperature,
                                      &humidity,
                                      &temperature_trend,
                                      &humidity_trend));
    assert(local_sensor_state_version() == version_before_failure + 1);
    assert(snapshot_matches(temperature, humidity,
                            temperature_trend, humidity_trend,
                            22.0f, 44.0f, -1, 1) ||
           snapshot_matches(temperature, humidity,
                            temperature_trend, humidity_trend,
                            11.0f, 33.0f, 1, -1));
    unavailable_changed = true;
    assert(local_sensor_state_publish_unavailable(&unavailable_changed));
    assert(!unavailable_changed);
    assert(local_sensor_state_version() == version_before_failure + 1);
    return 0;
}
