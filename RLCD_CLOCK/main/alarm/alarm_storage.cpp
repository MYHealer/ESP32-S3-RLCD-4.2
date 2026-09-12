// 实现闹钟 NVS 三键记录的读取、逐键条件写入和整命名空间清除。
#include "alarm_storage.h"

#include "scoped_nvs_handle.h"

namespace alarm_storage {
namespace {
using app_storage::ScopedNvsHandle;

constexpr const char *kNamespace = "alarm_v1";
constexpr const char *kEnabledKey = "enabled";
constexpr const char *kHourKey = "hour";
constexpr const char *kMinuteKey = "minute";

bool all_keys_missing(esp_err_t enabled_err, esp_err_t hour_err, esp_err_t minute_err)
{
    return enabled_err == ESP_ERR_NVS_NOT_FOUND &&
           hour_err == ESP_ERR_NVS_NOT_FOUND &&
           minute_err == ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t first_read_error(esp_err_t enabled_err, esp_err_t hour_err, esp_err_t minute_err)
{
    if (enabled_err != ESP_OK) {
        return enabled_err;
    }
    if (hour_err != ESP_OK) {
        return hour_err;
    }
    return minute_err;
}

esp_err_t write_u8_if_changed(nvs_handle_t nvs,
                              esp_err_t err,
                              const char *key,
                              uint8_t value,
                              bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    uint8_t stored = 0;
    if (nvs_get_u8(nvs, key, &stored) == ESP_OK && stored == value) {
        return ESP_OK;
    }
    err = nvs_set_u8(nvs, key, value);
    if (err == ESP_OK && changed) {
        *changed = true;
    }
    return err;
}
} // namespace

ReadResult read()
{
    ReadResult result = {};
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNamespace, NVS_READONLY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        result.status = ReadStatus::kEmpty;
        return result;
    }
    if (err != ESP_OK) {
        result.status = ReadStatus::kOpenFailed;
        result.error = err;
        return result;
    }

    esp_err_t enabled_err = nvs_get_u8(nvs.get(), kEnabledKey, &result.enabled);
    esp_err_t hour_err = nvs_get_u8(nvs.get(), kHourKey, &result.hour);
    esp_err_t minute_err = nvs_get_u8(nvs.get(), kMinuteKey, &result.minute);
    nvs.close();

    if (enabled_err == ESP_OK && hour_err == ESP_OK && minute_err == ESP_OK) {
        result.status = ReadStatus::kLoaded;
    } else if (all_keys_missing(enabled_err, hour_err, minute_err)) {
        result.status = ReadStatus::kEmpty;
    } else {
        result.status = ReadStatus::kIncomplete;
        result.error = first_read_error(enabled_err, hour_err, minute_err);
    }
    return result;
}

WriteResult write(bool enabled, uint8_t hour, uint8_t minute)
{
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNamespace, NVS_READWRITE);
    if (err != ESP_OK) {
        return {WriteStatus::kOpenFailed, err};
    }

    bool any_changed = false;
    bool item_changed = false;
    err = write_u8_if_changed(
        nvs.get(), err, kEnabledKey, enabled ? 1 : 0, &item_changed);
    any_changed = any_changed || item_changed;
    err = write_u8_if_changed(
        nvs.get(), err, kHourKey, hour, &item_changed);
    any_changed = any_changed || item_changed;
    err = write_u8_if_changed(
        nvs.get(), err, kMinuteKey, minute, &item_changed);
    any_changed = any_changed || item_changed;
    if (err == ESP_OK && any_changed) {
        err = nvs_commit(nvs.get());
    }
    nvs.close();
    return err == ESP_OK
               ? WriteResult{WriteStatus::kSaved, ESP_OK}
               : WriteResult{WriteStatus::kWriteFailed, err};
}

ClearResult clear()
{
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(kNamespace, NVS_READWRITE);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return {ClearStatus::kAlreadyEmpty, ESP_OK};
    }
    if (err != ESP_OK) {
        return {ClearStatus::kOpenFailed, err};
    }

    err = nvs_erase_all(nvs.get());
    if (err == ESP_OK) {
        err = nvs_commit(nvs.get());
    }
    nvs.close();
    return err == ESP_OK
               ? ClearResult{ClearStatus::kCleared, ESP_OK}
               : ClearResult{ClearStatus::kEraseFailed, err};
}

} // namespace alarm_storage
