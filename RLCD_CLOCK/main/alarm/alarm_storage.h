// 封装单次闹钟在独立 NVS 命名空间中的原始读写。
#pragma once

#include <stdint.h>

#include "esp_err.h"

namespace alarm_storage {

enum class ReadStatus {
    kLoaded,
    kEmpty,
    kIncomplete,
    kOpenFailed,
};

struct ReadResult {
    ReadStatus status = ReadStatus::kEmpty;
    esp_err_t error = ESP_OK;
    uint8_t enabled = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
};

enum class WriteStatus {
    kSaved,
    kOpenFailed,
    kWriteFailed,
};

struct WriteResult {
    WriteStatus status = WriteStatus::kSaved;
    esp_err_t error = ESP_OK;
};

enum class ClearStatus {
    kCleared,
    kAlreadyEmpty,
    kOpenFailed,
    kEraseFailed,
};

struct ClearResult {
    ClearStatus status = ClearStatus::kAlreadyEmpty;
    esp_err_t error = ESP_OK;
};

ReadResult read();
WriteResult write(bool enabled, uint8_t hour, uint8_t minute);
ClearResult clear();

} // namespace alarm_storage
