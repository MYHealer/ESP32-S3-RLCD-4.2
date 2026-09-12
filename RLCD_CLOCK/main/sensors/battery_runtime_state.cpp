// 集中发布电池相关字段，避免采样任务与消费者读取到混合状态。
#include "battery_runtime_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <atomic>

namespace {
constexpr uint32_t kBatteryStatusPercentMask = 0xffffu;
constexpr uint32_t kBatteryStatusPercentSignBit = 1u << 15;
constexpr int kBatteryStatusPercentRange = 1 << 16;
constexpr uint32_t kBatteryStatusChargingBit = 1u << 16;
constexpr uint32_t kBatteryStatusLowModeBit = 1u << 17;

constexpr uint32_t pack_battery_status(int percent,
                                       bool charging,
                                       bool low_battery_mode)
{
    return (static_cast<uint32_t>(percent) & kBatteryStatusPercentMask) |
           (charging ? kBatteryStatusChargingBit : 0u) |
           (low_battery_mode ? kBatteryStatusLowModeBit : 0u);
}

BatteryRuntimeStatusSnapshot unpack_battery_status(uint32_t packed)
{
    const uint32_t percent_bits = packed & kBatteryStatusPercentMask;
    const int percent = static_cast<int>(percent_bits) -
                        ((percent_bits & kBatteryStatusPercentSignBit) != 0
                             ? kBatteryStatusPercentRange
                             : 0);
    return {
        percent,
        (packed & kBatteryStatusChargingBit) != 0,
        (packed & kBatteryStatusLowModeBit) != 0,
    };
}

StaticTaskMutex s_battery_runtime_mutex;
BatteryRuntimeSnapshot s_battery_runtime;
std::atomic<uint32_t> s_battery_version{0};
std::atomic<uint32_t> s_battery_status{
    pack_battery_status(-1, false, false),
};

static_assert(kBatteryStatusPercentRange == 65536,
              "battery percent mirror requires a 16-bit signed field");
}

bool battery_runtime_state_init()
{
    return s_battery_runtime_mutex.init();
}

bool battery_runtime_snapshot_load(BatteryRuntimeSnapshot *out)
{
    if (!out) {
        return false;
    }
    ScopedSemaphoreLock lock(s_battery_runtime_mutex.handle());
    if (!lock) {
        return false;
    }
    *out = s_battery_runtime;
    return true;
}

void battery_runtime_snapshot_store(const BatteryRuntimeSnapshot &snapshot)
{
    ScopedSemaphoreLock lock(s_battery_runtime_mutex.handle());
    if (!lock) {
        return;
    }
    s_battery_runtime = snapshot;
    s_battery_status.store(pack_battery_status(snapshot.percent,
                                               snapshot.charging,
                                               snapshot.low_battery_mode),
                           std::memory_order_release);
    s_battery_version.store(snapshot.version, std::memory_order_release);
}

BatteryRuntimeStatusSnapshot battery_runtime_status_load()
{
    return unpack_battery_status(
        s_battery_status.load(std::memory_order_acquire));
}

uint32_t battery_runtime_version_load()
{
    return s_battery_version.load(std::memory_order_acquire);
}

int battery_percent_load()
{
    return battery_runtime_status_load().percent;
}

bool battery_low_mode_load()
{
    return battery_runtime_status_load().low_battery_mode;
}

bool battery_low_mode_for_percent(bool current_mode,
                                  int percent,
                                  int enter_percent,
                                  int exit_percent)
{
    if (percent < 0) {
        return current_mode;
    }
    if (!current_mode && percent < enter_percent) {
        return true;
    }
    if (current_mode && percent >= exit_percent) {
        return false;
    }
    return current_mode;
}
