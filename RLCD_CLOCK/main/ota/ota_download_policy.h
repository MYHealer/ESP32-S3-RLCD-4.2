// 统一定义 OTA 检查确认、HTTP 下载、进度刷新和超时参数。
#pragma once

#include <stddef.h>
#include <string.h>

inline constexpr int kOtaHttpTimeoutMs = 8 * 1000;
inline constexpr int kOtaNoProgressTimeoutMs = 45 * 1000;
inline constexpr int kOtaMaxDownloadMs = 10 * 60 * 1000;
inline constexpr int kOtaStatusMinIntervalMs = 3 * 1000;
inline constexpr int kOtaAvailableConfirmTimeoutMs = 60 * 1000;
inline constexpr int kOtaDownloadBufferSize = 4096;
inline constexpr int kOtaChunkDelayMs = 25;

enum class OtaInstallAttemptResult {
    kSucceeded,
    kRetryBackupSource,
    kTerminalFailure,
};

constexpr bool ota_install_attempt_succeeded(OtaInstallAttemptResult result)
{
    return result == OtaInstallAttemptResult::kSucceeded;
}

constexpr bool ota_install_backup_allowed(OtaInstallAttemptResult result)
{
    return result == OtaInstallAttemptResult::kRetryBackupSource;
}

static_assert(kOtaHttpTimeoutMs > 0, "OTA HTTP timeout must be positive");
static_assert(kOtaStatusMinIntervalMs > 0,
              "OTA progress interval must be positive");
static_assert(kOtaNoProgressTimeoutMs > kOtaStatusMinIntervalMs,
              "OTA no-progress timeout must exceed the progress interval");
static_assert(kOtaMaxDownloadMs > kOtaNoProgressTimeoutMs,
              "OTA download timeout must exceed the no-progress timeout");
static_assert(kOtaAvailableConfirmTimeoutMs > 0,
              "OTA confirmation timeout must be positive");
static_assert(kOtaDownloadBufferSize >= 1024,
              "OTA download buffer must fit a practical transfer chunk");
static_assert(kOtaChunkDelayMs > 0, "OTA chunk delay must be positive");

inline bool ota_download_url_copy_exact(const char *source,
                                        char *out,
                                        size_t out_len)
{
    if (!source || source[0] == '\0' || !out || out_len == 0) {
        return false;
    }
    const size_t source_len = strlen(source);
    if (source_len >= out_len) {
        return false;
    }
    memcpy(out, source, source_len + 1);
    return true;
}
