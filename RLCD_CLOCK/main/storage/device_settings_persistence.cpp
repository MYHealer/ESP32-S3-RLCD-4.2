// 负责声音、工作页、小智自动返回和图库周期设置的条件写入与 NVS 提交。
#include "device_settings_persistence.h"

#include "app_metadata.h"
#include "chime_runtime_state_internal.h"
#include "network_chime_storage.h"
#include "network_config_keys.h"
#include "network_config_nvs.h"
#include "network_page_storage.h"
#include "ui_gallery_rotation_state_internal.h"
#include "ui_work_page_catalog_internal.h"
#include "ui_work_page_order_policy.h"
#include "xiaozhi_auto_return_state_internal.h"

#include <esp_log.h>

using network_config_nvs::commit_nvs_if_changed;
using network_config_nvs::ScopedNvsHandle;
using network_config_nvs::write_changed_nvs_u8;
using network_page_storage::kPageMaskV6Key;
using network_page_storage::write_work_page_order_nvs;
using network_config_keys::kXiaozhiAutoReturnKey;
using network_config_keys::kGalleryRotationKey;

namespace {
constexpr const char *kNvsActionSavingHourlyReminder = "saving hourly reminder";
constexpr const char *kNvsActionSavingPageSettings = "saving page settings";
constexpr const char *kNvsActionSavingPageOrder = "saving page order";
constexpr const char *kNvsActionSavingXiaozhiAutoReturn = "saving Xiaozhi auto return";
constexpr const char *kNvsActionSavingGalleryRotation = "saving gallery rotation";
constexpr const char *kNvsFailureContextPageSettings = "page settings";
constexpr const char *kNvsFailureContextXiaozhiAutoReturn = "Xiaozhi auto return";
constexpr const char *kNvsFailureContextGalleryRotation = "gallery rotation";
#define NVS_SAVE_HOURLY_REMINDER_FAILED_FORMAT "nvs save hourly reminder failed: %s"
#define NVS_SAVE_PAGE_ORDER_FAILED_FORMAT "nvs save page order failed: %s"
#define NVS_SAVE_U8_SETTING_FAILED_FORMAT "nvs save %s failed: %s"

constexpr uint8_t bool_to_nvs_u8(bool value)
{
    return value ? 1 : 0;
}

bool save_chime_setting_snapshot(const ChimeRuntimeSnapshot &runtime)
{
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(NVS_READWRITE, kNvsActionSavingHourlyReminder);
    if (err != ESP_OK) {
        return false;
    }
    network_chime_storage::StoredChimeSettings settings = {};
    settings.enabled = bool_to_nvs_u8(runtime.hourly_enabled);
    settings.all_day = bool_to_nvs_u8(runtime.all_day);
    settings.volume = runtime.volume_percent;
    settings.sound = runtime.sound_index;
    bool changed = false;
    err = network_chime_storage::write_if_changed(nvs.get(), err, settings, &changed);
    err = commit_nvs_if_changed(nvs.get(), err, changed);
    if (!nvs.close_save_ok(err)) {
        ESP_LOGW(TAG, NVS_SAVE_HOURLY_REMINDER_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool save_changed_u8_setting(const char *action,
                             const char *failure_context,
                             const char *key,
                             uint8_t value)
{
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(NVS_READWRITE, action);
    if (err != ESP_OK) {
        return false;
    }
    bool changed = false;
    err = write_changed_nvs_u8(nvs.get(), err, key, value, &changed);
    err = commit_nvs_if_changed(nvs.get(), err, changed);
    if (!nvs.close_save_ok(err)) {
        ESP_LOGW(TAG,
                 NVS_SAVE_U8_SETTING_FAILED_FORMAT,
                 failure_context,
                 esp_err_to_name(err));
        return false;
    }
    return true;
}
} // namespace

bool save_hourly_chime_setting()
{
    return save_chime_setting_snapshot(chime_runtime_snapshot_load());
}

bool set_chime_setting(const ChimeRuntimeSnapshot &settings)
{
    if (!save_chime_setting_snapshot(settings)) {
        return false;
    }
    chime_runtime_snapshot_store(settings);
    return true;
}

bool set_work_page_enabled_mask_setting(uint8_t page_mask)
{
    const uint8_t mask = normalize_work_page_enabled_mask(page_mask);
    if (!save_changed_u8_setting(kNvsActionSavingPageSettings,
                                 kNvsFailureContextPageSettings,
                                 kPageMaskV6Key,
                                 mask)) {
        return false;
    }
    work_page_enabled_mask_store(mask);
    return true;
}

bool set_work_page_order_setting(const uint8_t *page_order,
                                 size_t page_order_size)
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    if (!work_page_order_policy::order_is_valid(page_order, page_order_size) ||
        !work_page_order_policy::order_has_valid_home(
            page_order, page_order_size, page_mask)) {
        return false;
    }
    ScopedNvsHandle nvs;
    esp_err_t err = nvs.open(NVS_READWRITE, kNvsActionSavingPageOrder);
    if (err != ESP_OK) {
        return false;
    }
    bool changed = false;
    err = write_work_page_order_nvs(nvs.get(),
                                    err,
                                    page_order,
                                    page_order_size,
                                    &changed);
    err = commit_nvs_if_changed(nvs.get(), err, changed);
    if (!nvs.close_save_ok(err)) {
        ESP_LOGW(TAG, NVS_SAVE_PAGE_ORDER_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    work_page_order_replace(page_order, page_order_size);
    return true;
}

bool set_xiaozhi_auto_return_setting(bool enabled)
{
    if (!save_changed_u8_setting(kNvsActionSavingXiaozhiAutoReturn,
                                 kNvsFailureContextXiaozhiAutoReturn,
                                 kXiaozhiAutoReturnKey,
                                 bool_to_nvs_u8(enabled))) {
        return false;
    }
    xiaozhi_auto_return_enabled_store(enabled);
    return true;
}

bool set_gallery_rotation_period_setting(uint8_t period)
{
    const uint8_t normalized = normalize_gallery_rotation_period(period);
    if (!save_changed_u8_setting(kNvsActionSavingGalleryRotation,
                                 kNvsFailureContextGalleryRotation,
                                 kGalleryRotationKey,
                                 normalized)) {
        return false;
    }
    gallery_rotation_period_store(normalized);
    return true;
}
