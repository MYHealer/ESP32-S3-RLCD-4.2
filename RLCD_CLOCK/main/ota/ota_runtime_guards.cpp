// 实现 OTA 下载期间的显示静默和任务看门狗资源生命周期。
#include "ota_runtime_guards.h"

#include "app_metadata.h"
#include "display_bsp.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#define OTA_TASK_WATCHDOG_SUBSCRIBE_SKIPPED_FORMAT "OTA task watchdog subscribe skipped: %s"
#define OTA_TASK_WATCHDOG_UNSUBSCRIBE_FAILED_FORMAT "OTA task watchdog unsubscribe failed: %s"

OtaDisplayQuietGuard::OtaDisplayQuietGuard()
{
    Display_SetOtaQuietMode(true);
}

OtaDisplayQuietGuard::~OtaDisplayQuietGuard()
{
    Display_SetOtaQuietMode(false);
}

OtaTaskWatchdogGuard::OtaTaskWatchdogGuard()
{
    if (esp_task_wdt_status(nullptr) == ESP_OK) {
        active_ = true;
        return;
    }
    esp_err_t err = esp_task_wdt_add(nullptr);
    if (err == ESP_OK) {
        active_ = true;
        added_ = true;
    } else {
        ESP_LOGW(TAG, OTA_TASK_WATCHDOG_SUBSCRIBE_SKIPPED_FORMAT, esp_err_to_name(err));
    }
}

OtaTaskWatchdogGuard::~OtaTaskWatchdogGuard()
{
    if (!added_) {
        return;
    }
    esp_err_t err = esp_task_wdt_delete(nullptr);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, OTA_TASK_WATCHDOG_UNSUBSCRIBE_FAILED_FORMAT, esp_err_to_name(err));
    }
}

void OtaTaskWatchdogGuard::reset()
{
    if (active_) {
        esp_task_wdt_reset();
    }
}

OtaWriteHandleGuard::OtaWriteHandleGuard(esp_ota_handle_t handle)
    : handle_(handle)
{
}

OtaWriteHandleGuard::~OtaWriteHandleGuard()
{
    if (handle_ != 0) {
        esp_ota_abort(handle_);
    }
}

esp_err_t OtaWriteHandleGuard::finish()
{
    const esp_ota_handle_t handle = handle_;
    handle_ = 0;
    if (handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_ota_end(handle);
}
