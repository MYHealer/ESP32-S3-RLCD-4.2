// 集中定义整点提醒音量档位、默认值和纯归一化/循环选择规则。
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace chime_settings {

inline constexpr int kVolumeLevels[] = {20, 40, 60, 80, 100};
inline constexpr size_t kVolumeLevelCount = sizeof(kVolumeLevels) / sizeof(kVolumeLevels[0]);
inline constexpr int kDefaultVolumePercent = 80;
inline constexpr uint8_t kSoundCount = 4;

constexpr bool volume_levels_ordered_and_bounded()
{
    int previous = 0;
    for (int volume : kVolumeLevels) {
        if (volume <= 0 || volume > 100 || volume <= previous) {
            return false;
        }
        previous = volume;
    }
    return true;
}

constexpr bool volume_levels_include_default()
{
    for (int volume : kVolumeLevels) {
        if (volume == kDefaultVolumePercent) {
            return true;
        }
    }
    return false;
}

constexpr uint8_t normalize_stored_volume(uint8_t volume)
{
    return volume <= 100 ? volume : static_cast<uint8_t>(kDefaultVolumePercent);
}

constexpr int next_volume_percent(int current)
{
    for (size_t i = 0; i < kVolumeLevelCount; ++i) {
        if (current < kVolumeLevels[i]) {
            return kVolumeLevels[i];
        }
        if (current == kVolumeLevels[i]) {
            return kVolumeLevels[(i + 1) % kVolumeLevelCount];
        }
    }
    return kVolumeLevels[0];
}

static_assert(kVolumeLevelCount > 0, "chime volume level list must not be empty");
static_assert(kSoundCount > 0, "chime sound list must not be empty");
static_assert(volume_levels_ordered_and_bounded(),
              "chime volume levels must be ordered percentages in 1..100");
static_assert(volume_levels_include_default(), "default chime volume must be selectable");

} // namespace chime_settings
