// 声明 OTA 下载期间复用的显示、看门狗和写入句柄作用域守卫。
#pragma once

#include <esp_err.h>
#include <esp_ota_ops.h>

class OtaDisplayQuietGuard {
public:
    OtaDisplayQuietGuard();
    ~OtaDisplayQuietGuard();

    OtaDisplayQuietGuard(const OtaDisplayQuietGuard &) = delete;
    OtaDisplayQuietGuard &operator=(const OtaDisplayQuietGuard &) = delete;
};

class OtaTaskWatchdogGuard {
public:
    OtaTaskWatchdogGuard();
    ~OtaTaskWatchdogGuard();

    OtaTaskWatchdogGuard(const OtaTaskWatchdogGuard &) = delete;
    OtaTaskWatchdogGuard &operator=(const OtaTaskWatchdogGuard &) = delete;

    void reset();

private:
    bool active_ = false;
    bool added_ = false;
};

class OtaWriteHandleGuard {
public:
    explicit OtaWriteHandleGuard(esp_ota_handle_t handle);
    ~OtaWriteHandleGuard();

    OtaWriteHandleGuard(const OtaWriteHandleGuard &) = delete;
    OtaWriteHandleGuard &operator=(const OtaWriteHandleGuard &) = delete;

    esp_ota_handle_t handle() const
    {
        return handle_;
    }

    esp_err_t finish();

private:
    esp_ota_handle_t handle_ = 0;
};
