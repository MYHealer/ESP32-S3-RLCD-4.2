// 验证小智天气城市待保存快照的并发完整性与代次清理语义。
#include "weather_city_pending_state_internal.h"

#include <assert.h>
#include <atomic>
#include <string.h>
#include <thread>

int main()
{
    WeatherCityPendingSnapshot snapshot = {};
    assert(!weather_city_pending_snapshot(&snapshot));
    assert(!weather_city_pending_exists());
    assert(!weather_city_pending_store("杭州"));
    assert(!weather_city_pending_clear(1));

    assert(weather_city_pending_state_init());
    assert(weather_city_pending_state_init());
    assert(weather_city_pending_snapshot(&snapshot));
    assert(!snapshot.pending);
    assert(snapshot.city[0] == '\0');
    assert(snapshot.generation == 0);

    assert(weather_city_pending_store("杭州"));
    assert(weather_city_pending_snapshot(&snapshot));
    assert(snapshot.pending);
    assert(strcmp(snapshot.city, "杭州") == 0);
    const uint32_t first_generation = snapshot.generation;
    assert(first_generation != 0);

    assert(weather_city_pending_store("杭州"));
    assert(!weather_city_pending_clear(first_generation));
    assert(weather_city_pending_snapshot(&snapshot));
    assert(snapshot.pending);
    assert(strcmp(snapshot.city, "杭州") == 0);
    assert(snapshot.generation != first_generation);

    constexpr const char *kCityA = "杭州";
    constexpr const char *kCityB = "上海";
    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            assert(weather_city_pending_store((i & 1) ? kCityA : kCityB));
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        assert(weather_city_pending_snapshot(&snapshot));
        assert(snapshot.pending);
        assert(strcmp(snapshot.city, kCityA) == 0 ||
               strcmp(snapshot.city, kCityB) == 0);
        assert(snapshot.generation != 0);
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();

    assert(weather_city_pending_snapshot(&snapshot));
    assert(weather_city_pending_clear(snapshot.generation));
    assert(!weather_city_pending_exists());
    assert(weather_city_pending_snapshot(&snapshot));
    assert(!snapshot.pending);
    assert(snapshot.city[0] == '\0');
    assert(!weather_city_pending_clear(snapshot.generation));
    return 0;
}
