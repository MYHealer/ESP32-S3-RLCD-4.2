// 实现整点提醒配置的成组 NVS 读取、比较和条件写入。
#include "network_chime_storage.h"

#include "network_config_nvs.h"

namespace network_chime_storage {

StoredChimeSettings read(nvs_handle_t nvs, uint8_t default_volume)
{
    StoredChimeSettings settings = {};
    settings.enabled = network_config_nvs::read_nvs_u8_or_default(nvs, kHourlyChimeKey, 0);
    settings.all_day = network_config_nvs::read_nvs_u8_or_default(nvs, kHourlyAllDayKey, 0);
    settings.volume = network_config_nvs::read_nvs_u8_or_default(nvs,
                                                                 kChimeVolumeKey,
                                                                 default_volume);
    settings.sound = network_config_nvs::read_nvs_u8_or_default(nvs, kChimeSoundKey, 0);
    return settings;
}

bool matches(nvs_handle_t nvs, const StoredChimeSettings &settings)
{
    uint8_t enabled = 0;
    uint8_t all_day = 0;
    uint8_t volume = 0;
    uint8_t sound = 0;
    return nvs_get_u8(nvs, kHourlyChimeKey, &enabled) == ESP_OK &&
           nvs_get_u8(nvs, kHourlyAllDayKey, &all_day) == ESP_OK &&
           nvs_get_u8(nvs, kChimeVolumeKey, &volume) == ESP_OK &&
           nvs_get_u8(nvs, kChimeSoundKey, &sound) == ESP_OK &&
           enabled == settings.enabled &&
           all_day == settings.all_day &&
           volume == settings.volume &&
           sound == settings.sound;
}

esp_err_t write_if_changed(nvs_handle_t nvs,
                           esp_err_t err,
                           const StoredChimeSettings &settings,
                           bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    bool any_changed = false;
    bool item_changed = false;
    err = network_config_nvs::write_changed_nvs_u8(
        nvs, err, kHourlyChimeKey, settings.enabled, &item_changed);
    any_changed = any_changed || item_changed;
    err = network_config_nvs::write_changed_nvs_u8(
        nvs, err, kHourlyAllDayKey, settings.all_day, &item_changed);
    any_changed = any_changed || item_changed;
    err = network_config_nvs::write_changed_nvs_u8(
        nvs, err, kChimeVolumeKey, settings.volume, &item_changed);
    any_changed = any_changed || item_changed;
    err = network_config_nvs::write_changed_nvs_u8(
        nvs, err, kChimeSoundKey, settings.sound, &item_changed);
    any_changed = any_changed || item_changed;
    if (changed) {
        *changed = err == ESP_OK && any_changed;
    }
    return err;
}

} // namespace network_chime_storage
