// 使用单个 32 位原子值发布声音设置，避免跨任务读取到混合配置。
#include "chime_runtime_state_internal.h"

#include "chime_settings.h"

#include <atomic>

namespace {
constexpr uint32_t kHourlyEnabledMask = 1U << 0;
constexpr uint32_t kAllDayMask = 1U << 1;
constexpr uint32_t kVolumeShift = 8;
constexpr uint32_t kSoundShift = 16;
constexpr uint32_t kByteMask = 0xffU;

constexpr uint8_t narrow_to_byte(int value)
{
    return static_cast<uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

constexpr uint32_t pack_snapshot(const ChimeRuntimeSnapshot &snapshot)
{
    return (snapshot.hourly_enabled ? kHourlyEnabledMask : 0U) |
           (snapshot.all_day ? kAllDayMask : 0U) |
           (static_cast<uint32_t>(snapshot.volume_percent) << kVolumeShift) |
           (static_cast<uint32_t>(snapshot.sound_index) << kSoundShift);
}

constexpr ChimeRuntimeSnapshot unpack_snapshot(uint32_t packed)
{
    return {
        (packed & kHourlyEnabledMask) != 0,
        (packed & kAllDayMask) != 0,
        static_cast<uint8_t>((packed >> kVolumeShift) & kByteMask),
        static_cast<uint8_t>((packed >> kSoundShift) & kByteMask),
    };
}

constexpr ChimeRuntimeSnapshot kDefaultSnapshot = {
    false,
    false,
    static_cast<uint8_t>(chime_settings::kDefaultVolumePercent),
    0,
};

std::atomic<uint32_t> s_chime_runtime{pack_snapshot(kDefaultSnapshot)};

void update_field(uint32_t clear_mask, uint32_t value)
{
    uint32_t current = s_chime_runtime.load(std::memory_order_relaxed);
    uint32_t desired = 0;
    do {
        desired = (current & ~clear_mask) | (value & clear_mask);
    } while (!s_chime_runtime.compare_exchange_weak(current,
                                                    desired,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));
}
} // namespace

ChimeRuntimeSnapshot chime_runtime_snapshot_load()
{
    return unpack_snapshot(s_chime_runtime.load(std::memory_order_acquire));
}

void chime_runtime_snapshot_store(const ChimeRuntimeSnapshot &snapshot)
{
    s_chime_runtime.store(pack_snapshot(snapshot), std::memory_order_release);
}

bool chime_runtime_any_enabled()
{
    constexpr uint32_t kAnyEnabledMask = kHourlyEnabledMask | kAllDayMask;
    return (s_chime_runtime.load(std::memory_order_acquire) & kAnyEnabledMask) != 0;
}

int chime_runtime_volume_percent()
{
    return static_cast<int>(
        (s_chime_runtime.load(std::memory_order_acquire) >> kVolumeShift) & kByteMask);
}

int chime_runtime_sound_index()
{
    return static_cast<int>(
        (s_chime_runtime.load(std::memory_order_acquire) >> kSoundShift) & kByteMask);
}

void chime_runtime_volume_percent_store(int volume_percent)
{
    update_field(kByteMask << kVolumeShift,
                 static_cast<uint32_t>(narrow_to_byte(volume_percent)) << kVolumeShift);
}
