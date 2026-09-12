// 集中管理整点提醒四项 NVS key、默认读取和无变化跳写规则。
#pragma once

#include "esp_err.h"
#include "nvs.h"

#include <stdint.h>

namespace network_chime_storage {

inline constexpr const char *kHourlyChimeKey = "hourly_chime_v2";
inline constexpr const char *kHourlyAllDayKey = "hour_all_v1";
inline constexpr const char *kChimeVolumeKey = "chime_vol_v1";
inline constexpr const char *kChimeSoundKey = "chime_snd_v1";

struct StoredChimeSettings {
    uint8_t enabled = 0;
    uint8_t all_day = 0;
    uint8_t volume = 0;
    uint8_t sound = 0;
};

StoredChimeSettings read(nvs_handle_t nvs, uint8_t default_volume);
bool matches(nvs_handle_t nvs, const StoredChimeSettings &settings);
esp_err_t write_if_changed(nvs_handle_t nvs,
                           esp_err_t err,
                           const StoredChimeSettings &settings,
                           bool *changed);

} // namespace network_chime_storage
