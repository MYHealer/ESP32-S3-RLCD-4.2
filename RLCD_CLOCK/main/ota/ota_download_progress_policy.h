// 定义 OTA 下载进度诊断与状态刷新节流的纯规则。
#pragma once

#include <stdint.h>

struct OtaDownloadProgressState {
    int last_noted_progress = -1;
    int last_heap_progress = -25;
    int64_t last_status_us = 0;
    int last_status_total = 0;
};

constexpr int ota_download_progress_percent(int total, int expected)
{
    if (expected <= 0 || total <= 0) {
        return 0;
    }
    const int64_t progress = static_cast<int64_t>(total) * 100 / expected;
    return progress > 100 ? 100 : static_cast<int>(progress);
}

constexpr bool ota_download_progress_note_due(
    const OtaDownloadProgressState &state,
    int progress)
{
    return progress != state.last_noted_progress;
}

inline void ota_download_progress_mark_noted(OtaDownloadProgressState &state,
                                             int progress)
{
    state.last_noted_progress = progress;
}

constexpr bool ota_download_heap_log_due(
    const OtaDownloadProgressState &state,
    int progress)
{
    return progress >= state.last_heap_progress + 25 ||
           (progress >= 100 && state.last_heap_progress < 100);
}

inline void ota_download_progress_mark_heap_logged(
    OtaDownloadProgressState &state,
    int progress)
{
    state.last_heap_progress = progress;
}

constexpr bool ota_download_status_due(const OtaDownloadProgressState &state,
                                       int64_t now_us,
                                       int64_t interval_us,
                                       int progress)
{
    return state.last_status_us == 0 ||
           now_us - state.last_status_us >= interval_us ||
           progress >= 100;
}

constexpr int ota_download_status_window_bytes(
    const OtaDownloadProgressState &state,
    int total)
{
    return total - state.last_status_total;
}

constexpr int64_t ota_download_status_window_us(
    const OtaDownloadProgressState &state,
    int64_t started_us,
    int64_t now_us)
{
    return state.last_status_us == 0
               ? now_us - started_us
               : now_us - state.last_status_us;
}

inline void ota_download_progress_mark_status_published(
    OtaDownloadProgressState &state,
    int64_t now_us,
    int total)
{
    state.last_status_us = now_us;
    state.last_status_total = total;
}

