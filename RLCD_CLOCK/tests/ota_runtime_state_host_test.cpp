// 验证 OTA 运行态快照、下载状态和提示保持到期复位。
#include "ota_runtime_state_internal.h"

#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>

int g_network_runtime_notification_count = 0;
int g_housekeeping_notification_count = 0;

int main()
{
    OtaRuntimeTimingSnapshot timing_before_init;
    memset(&timing_before_init, 0xa5, sizeof(timing_before_init));
    ota_runtime_timing_snapshot_load(&timing_before_init);
    assert(timing_before_init.state == kOtaIdle);
    assert(timing_before_init.status_until_tick == 0);
    assert(!timing_before_init.status_hold_set);

    OtaRuntimeSnapshot before_init;
    memset(&before_init, 0xa5, sizeof(before_init));
    ota_runtime_snapshot_load(&before_init);
    assert(before_init.state == kOtaIdle);
    assert(before_init.progress == -1);
    assert(before_init.speed_kbps == -1);
    assert(before_init.status_until_tick == 0);
    assert(!before_init.status_hold_set);
    assert(!before_init.reboot_pending);
    assert(before_init.status[0] == '\0');

    assert(ota_runtime_state_init());
    assert(ota_runtime_state_init());

    OtaRuntimeSnapshot initial;
    ota_runtime_snapshot_load(nullptr);
    ota_runtime_snapshot_load(&initial);
    assert(initial.state == kOtaIdle);
    assert(initial.progress == -1);
    assert(initial.speed_kbps == -1);
    assert(!initial.status_hold_set);
    assert(!initial.reboot_pending);
    assert(strcmp(initial.status, "BOOT: Check Update") == 0);
    assert(g_network_runtime_notification_count == 0);
    assert(g_housekeeping_notification_count == 0);

    ota_runtime_publish_status(kOtaAvailable, "New version v2", -1, 200, true);
    OtaRuntimeSnapshot available;
    ota_runtime_snapshot_load(&available);
    assert(available.state == kOtaAvailable);
    assert(available.status_hold_set);
    assert(available.status_until_tick == 200);
    assert(strcmp(available.status, "New version v2") == 0);
    OtaRuntimeTimingSnapshot available_timing;
    ota_runtime_timing_snapshot_load(&available_timing);
    assert(available_timing.state == kOtaAvailable);
    assert(available_timing.status_until_tick == 200);
    assert(available_timing.status_hold_set);
    const OtaRuntimeFlagsSnapshot available_flags =
        ota_runtime_flags_load();
    assert(available_flags.state == kOtaAvailable);
    assert(!available_flags.reboot_pending);
    assert(ota_runtime_state_load() == kOtaAvailable);
    assert(g_network_runtime_notification_count == 0);
    assert(g_housekeeping_notification_count == 1);

    assert(ota_runtime_state_init());
    OtaRuntimeSnapshot available_after_reinit;
    ota_runtime_snapshot_load(&available_after_reinit);
    assert(available_after_reinit.state == kOtaAvailable);
    assert(available_after_reinit.status_hold_set);
    assert(available_after_reinit.status_until_tick == 200);
    assert(strcmp(available_after_reinit.status, "New version v2") == 0);
    assert(g_network_runtime_notification_count == 0);
    assert(g_housekeeping_notification_count == 1);

    ota_runtime_reset_status_if_idle(199, "idle");
    assert(ota_runtime_state_load() == kOtaAvailable);
    assert(g_housekeeping_notification_count == 1);

    ota_runtime_publish_download_status("Installing 25%", 25, 128);
    OtaRuntimeSnapshot downloading;
    ota_runtime_snapshot_load(&downloading);
    assert(downloading.state == kOtaUpdating);
    assert(downloading.progress == 25);
    assert(downloading.speed_kbps == 128);
    assert(!downloading.status_hold_set);
    OtaRuntimeTimingSnapshot downloading_timing;
    ota_runtime_timing_snapshot_load(&downloading_timing);
    assert(downloading_timing.state == kOtaUpdating);
    assert(downloading_timing.status_until_tick == 0);
    assert(!downloading_timing.status_hold_set);
    assert(g_network_runtime_notification_count == 1);
    assert(g_housekeeping_notification_count == 2);

    ota_runtime_reboot_pending_store(true);
    const OtaRuntimeFlagsSnapshot reboot_flags =
        ota_runtime_flags_load();
    assert(reboot_flags.state == kOtaUpdating);
    assert(reboot_flags.reboot_pending);
    ota_runtime_publish_status(kOtaFailed, "failed", -1, 300, true);
    OtaRuntimeSnapshot failed;
    ota_runtime_snapshot_load(&failed);
    assert(!failed.reboot_pending);
    assert(failed.speed_kbps == 128);
    assert(g_network_runtime_notification_count == 2);
    assert(g_housekeeping_notification_count == 3);

    char long_status[kOtaStatusLen + 16];
    memset(long_status, 'x', sizeof(long_status));
    long_status[sizeof(long_status) - 1] = '\0';
    ota_runtime_publish_status(kOtaFailed, long_status, -1, 400, true);
    OtaRuntimeSnapshot truncated;
    ota_runtime_snapshot_load(&truncated);
    assert(strlen(truncated.status) == kOtaStatusLen - 1);
    assert(g_network_runtime_notification_count == 2);
    assert(g_housekeeping_notification_count == 4);

    ota_runtime_reset_status_if_idle(400, "BOOT: Check Update");
    OtaRuntimeSnapshot idle;
    ota_runtime_snapshot_load(&idle);
    assert(idle.state == kOtaIdle);
    assert(idle.progress == -1);
    assert(idle.speed_kbps == -1);
    assert(!idle.status_hold_set);
    assert(idle.status_until_tick == 0);
    assert(strcmp(idle.status, "BOOT: Check Update") == 0);
    assert(g_network_runtime_notification_count == 2);
    assert(g_housekeeping_notification_count == 5);

    ota_runtime_publish_status(kOtaFailed, "persistent", -1, 0, false);
    ota_runtime_reset_status_if_idle(1000, "idle");
    assert(ota_runtime_state_load() == kOtaFailed);
    assert(idle.state == kOtaIdle);
    assert(g_network_runtime_notification_count == 2);
    assert(g_housekeeping_notification_count == 6);

    std::atomic<bool> writer_done{false};
    std::thread writer([&writer_done]() {
        char status[32] = {};
        for (int iteration = 0; iteration < 5000; ++iteration) {
            int progress = iteration % 100;
            snprintf(status, sizeof(status), "%d", progress);
            ota_runtime_publish_download_status(status,
                                                progress,
                                                progress + 100);
        }
        writer_done.store(true);
    });
    std::thread reader([&writer_done]() {
        while (!writer_done.load()) {
            OtaRuntimeSnapshot snapshot;
            ota_runtime_snapshot_load(&snapshot);
            if (snapshot.state != kOtaUpdating) {
                continue;
            }
            int status_progress = -1;
            assert(sscanf(snapshot.status, "%d", &status_progress) == 1);
            assert(status_progress == snapshot.progress);
            assert(snapshot.speed_kbps == snapshot.progress + 100);
        }
    });
    writer.join();
    reader.join();

    OtaRuntimeSnapshot final_snapshot;
    ota_runtime_snapshot_load(&final_snapshot);
    assert(final_snapshot.state == kOtaUpdating);
    assert(final_snapshot.progress == 99);
    assert(final_snapshot.speed_kbps == 199);
    assert(strcmp(final_snapshot.status, "99") == 0);
    const OtaRuntimeFlagsSnapshot final_flags = ota_runtime_flags_load();
    assert(final_flags.state == kOtaUpdating);
    assert(!final_flags.reboot_pending);
    assert(g_network_runtime_notification_count == 3);
    assert(g_housekeeping_notification_count == 7);
    return 0;
}
