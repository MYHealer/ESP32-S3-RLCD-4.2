// 验证 OTA 下载进度、诊断节点和状态刷新窗口的纯规则。
#include "ota_download_progress_policy.h"

#include <assert.h>

int main()
{
    assert(ota_download_progress_percent(0, 100) == 0);
    assert(ota_download_progress_percent(25, 100) == 25);
    assert(ota_download_progress_percent(150, 100) == 100);
    assert(ota_download_progress_percent(100, 0) == 0);

    OtaDownloadProgressState state;
    assert(ota_download_progress_note_due(state, 0));
    ota_download_progress_mark_noted(state, 0);
    assert(!ota_download_progress_note_due(state, 0));
    assert(ota_download_progress_note_due(state, 1));

    assert(ota_download_heap_log_due(state, 0));
    ota_download_progress_mark_heap_logged(state, 0);
    assert(!ota_download_heap_log_due(state, 24));
    assert(ota_download_heap_log_due(state, 25));
    ota_download_progress_mark_heap_logged(state, 100);
    assert(!ota_download_heap_log_due(state, 100));

    constexpr int64_t kStartedUs = 1000000;
    constexpr int64_t kIntervalUs = 3000000;
    assert(ota_download_status_due(state, kStartedUs, kIntervalUs, 0));
    assert(ota_download_status_window_bytes(state, 2048) == 2048);
    assert(ota_download_status_window_us(state, kStartedUs, 2000000) == 1000000);

    ota_download_progress_mark_status_published(state, 2000000, 2048);
    assert(!ota_download_status_due(state, 4999999, kIntervalUs, 50));
    assert(ota_download_status_due(state, 5000000, kIntervalUs, 50));
    assert(ota_download_status_due(state, 2000001, kIntervalUs, 100));
    assert(ota_download_status_window_bytes(state, 4096) == 2048);
    assert(ota_download_status_window_us(state, kStartedUs, 3500000) == 1500000);
    return 0;
}

