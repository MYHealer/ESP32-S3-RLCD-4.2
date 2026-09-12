// 验证电池运行态生产模块的一致快照和低电量迟滞规则。
#include "battery_runtime_state_internal.h"

#include <atomic>
#include <cassert>
#include <thread>

namespace {
constexpr int kIterations = 50000;
constexpr int kEnterPercent = 10;
constexpr int kExitPercent = 13;
}

std::atomic<bool> g_fail_battery_mutex_take{false};

int main()
{
    BatteryRuntimeSnapshot snapshot;
    BatteryRuntimeStatusSnapshot status = battery_runtime_status_load();
    assert(status.percent == -1);
    assert(!status.charging);
    assert(!status.low_battery_mode);
    assert(battery_percent_load() == -1);
    assert(!battery_low_mode_load());
    assert(battery_runtime_version_load() == 0);
    assert(!battery_runtime_snapshot_load(&snapshot));
    assert(snapshot.percent == -1);

    assert(battery_runtime_state_init());
    assert(battery_runtime_state_init());
    assert(battery_runtime_snapshot_load(&snapshot));
    assert(snapshot.percent == -1);
    assert(snapshot.voltage == -1.0f);
    assert(!snapshot.charging);
    assert(!snapshot.low_battery_mode);

    assert(!battery_low_mode_for_percent(false, -1, kEnterPercent, kExitPercent));
    assert(battery_low_mode_for_percent(false, 9, kEnterPercent, kExitPercent));
    assert(battery_low_mode_for_percent(true, 12, kEnterPercent, kExitPercent));
    assert(!battery_low_mode_for_percent(true, 13, kEnterPercent, kExitPercent));

    snapshot.percent = 9;
    snapshot.low_battery_mode = true;
    snapshot.version = 1;
    battery_runtime_snapshot_store(snapshot);
    assert(battery_low_mode_load());
    assert(battery_runtime_version_load() == snapshot.version);

    snapshot.percent = 50;
    snapshot.voltage = 50.0f;
    snapshot.charging = true;
    snapshot.animation_complete = true;
    snapshot.last_full_charge_time = 1234;
    snapshot.version = 7;
    snapshot.low_battery_mode = false;
    battery_runtime_snapshot_store(snapshot);
    assert(battery_percent_load() == 50);
    assert(!battery_low_mode_load());
    assert(battery_runtime_version_load() == snapshot.version);
    status = battery_runtime_status_load();
    assert(status.percent == snapshot.percent);
    assert(status.charging == snapshot.charging);
    assert(status.low_battery_mode == snapshot.low_battery_mode);

    BatteryRuntimeSnapshot preserved = snapshot;
    preserved.percent = 42;
    preserved.voltage = 4.2f;
    preserved.version = 42;
    g_fail_battery_mutex_take.store(true, std::memory_order_release);
    assert(!battery_runtime_snapshot_load(&preserved));
    g_fail_battery_mutex_take.store(false, std::memory_order_release);
    assert(preserved.percent == 42);
    assert(preserved.voltage == 4.2f);
    assert(preserved.version == 42);

    snapshot.charging = false;
    snapshot.animation_complete = false;
    snapshot.last_full_charge_time = 50;
    battery_runtime_snapshot_store(snapshot);

    std::atomic<bool> inconsistent{false};
    std::atomic<bool> status_inconsistent{false};
    std::thread writer([] {
        for (int i = 0; i < kIterations; ++i) {
            BatteryRuntimeSnapshot next;
            next.percent = i % 101;
            next.voltage = static_cast<float>(next.percent);
            next.charging = (next.percent % 2) != 0;
            next.animation_complete = next.charging;
            next.last_full_charge_time = next.percent;
            next.version = static_cast<uint32_t>(i);
            next.low_battery_mode = next.percent < kEnterPercent;
            battery_runtime_snapshot_store(next);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            BatteryRuntimeSnapshot current;
            if (!battery_runtime_snapshot_load(&current) ||
                current.voltage != static_cast<float>(current.percent) ||
                current.animation_complete != current.charging ||
                current.last_full_charge_time != current.percent) {
                inconsistent.store(true, std::memory_order_relaxed);
            }
        }
    });
    std::thread status_reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            const BatteryRuntimeStatusSnapshot current =
                battery_runtime_status_load();
            if (current.percent < 0 || current.percent > 100 ||
                current.charging != ((current.percent % 2) != 0) ||
                current.low_battery_mode != (current.percent < kEnterPercent)) {
                status_inconsistent.store(true, std::memory_order_relaxed);
            }
        }
    });
    writer.join();
    reader.join();
    status_reader.join();
    assert(!inconsistent.load(std::memory_order_relaxed));
    assert(!status_inconsistent.load(std::memory_order_relaxed));

    assert(battery_runtime_snapshot_load(&snapshot));
    assert(battery_percent_load() == snapshot.percent);
    assert(battery_low_mode_load() == snapshot.low_battery_mode);
    assert(battery_runtime_version_load() == snapshot.version);
    status = battery_runtime_status_load();
    assert(status.percent == snapshot.percent);
    assert(status.charging == snapshot.charging);
    assert(status.low_battery_mode == snapshot.low_battery_mode);
    return 0;
}
