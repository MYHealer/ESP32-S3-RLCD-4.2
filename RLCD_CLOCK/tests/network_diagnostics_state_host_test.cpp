// 验证网络检测状态与逐行文本快照的边界和独立性。
#include "network_diagnostics_state_internal.h"

#include <assert.h>
#include <atomic>
#include <string.h>
#include <thread>

std::atomic<bool> g_fail_mutex_take{false};

int main()
{
    static_assert(sizeof(NetworkDiagnosticsSnapshot) >=
                      static_cast<size_t>(kNetworkDiagLineCount * kNetworkDiagLineLen),
                  "network diagnostics snapshot must include every result line");
    assert(network_diagnostics_state_init());
    assert(network_diagnostics_state_init());

    const NetworkDiagPageRequestSnapshot initial_page =
        network_diag_page_snapshot_load();
    assert(!initial_page.requested);
    assert(initial_page.revision == 0);
    assert(!network_diag_page_requested());
    network_diag_page_request();
    assert(network_diag_page_requested());
    const NetworkDiagPageRequestSnapshot first_page =
        network_diag_page_snapshot_load();
    assert(first_page.requested);
    assert(first_page.revision == 1);
    network_diag_page_request();
    assert(!network_diag_page_clear_if_current(first_page));
    assert(network_diag_page_requested());
    const NetworkDiagPageRequestSnapshot second_page =
        network_diag_page_snapshot_load();
    assert(second_page.requested);
    assert(second_page.revision == first_page.revision + 1);
    NetworkDiagPageRequestSnapshot invalid_page = second_page;
    invalid_page.requested = false;
    assert(!network_diag_page_clear_if_current(invalid_page));
    assert(network_diag_page_clear_if_current(second_page));
    assert(!network_diag_page_requested());
    assert(!network_diag_page_clear_if_current(second_page));

    NetworkDiagnosticsSnapshot snapshot;
    assert(!network_diag_snapshot_load(nullptr));
    assert(network_diag_snapshot_load(&snapshot));
    assert(snapshot.state == kNetworkDiagIdle);
    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        assert(snapshot.lines[index][0] == '\0');
    }

    const char *waiting_formats[kNetworkDiagLineCount] = {
        "line0:%s",
        "line1:%s",
        "line2:%s",
        "line3:%s",
        "line4:%s",
        "line5:%s",
        "line6:%s",
        "line7:%s",
        "line8:%s",
    };
    assert(!network_diag_state_begin(nullptr,
                                     kNetworkDiagLineCount,
                                     "waiting"));
    assert(!network_diag_state_begin(waiting_formats,
                                     kNetworkDiagLineCount - 1,
                                     "waiting"));
    assert(network_diag_state_begin(waiting_formats,
                                    kNetworkDiagLineCount,
                                    "waiting"));
    assert(network_diag_snapshot_load(&snapshot));
    assert(snapshot.state == kNetworkDiagRunning);
    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        char expected[kNetworkDiagLineLen] = {};
        snprintf(expected,
                 sizeof(expected),
                 waiting_formats[index],
                 "waiting");
        assert(strcmp(snapshot.lines[index], expected) == 0);
    }

    const char *terminal_lines[kNetworkDiagLineCount] = {
        "done0",
        "done1",
        "done2",
        "done3",
        "done4",
        "done5",
        "done6",
        "done7",
        "done8",
    };
    assert(!network_diag_state_publish(nullptr,
                                       kNetworkDiagLineCount,
                                       kNetworkDiagDone));
    assert(!network_diag_state_publish(terminal_lines,
                                       kNetworkDiagLineCount - 1,
                                       kNetworkDiagDone));
    const char *invalid_terminal_lines[kNetworkDiagLineCount] = {};
    assert(!network_diag_state_publish(invalid_terminal_lines,
                                       kNetworkDiagLineCount,
                                       kNetworkDiagDone));
    assert(network_diag_state_publish(terminal_lines,
                                      kNetworkDiagLineCount,
                                      kNetworkDiagDone));
    assert(network_diag_snapshot_load(&snapshot));
    assert(snapshot.state == kNetworkDiagDone);
    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        assert(strcmp(snapshot.lines[index], terminal_lines[index]) == 0);
    }

    NetworkDiagnosticsSnapshot preserved = snapshot;
    g_fail_mutex_take.store(true, std::memory_order_release);
    assert(!network_diag_snapshot_load(&preserved));
    g_fail_mutex_take.store(false, std::memory_order_release);
    assert(preserved.state == kNetworkDiagDone);
    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        assert(strcmp(preserved.lines[index], terminal_lines[index]) == 0);
    }

    network_diag_state_store(kNetworkDiagRunning);
    network_diag_line_store(kNetworkDiagLocalIpLine, "本地IP: 192.168.1.10");
    network_diag_line_store(-1, "ignored");
    network_diag_line_store(kNetworkDiagLineCount, "ignored");
    assert(network_diag_snapshot_load(&snapshot));
    assert(snapshot.state == kNetworkDiagRunning);
    assert(strcmp(snapshot.lines[kNetworkDiagLocalIpLine],
                  "本地IP: 192.168.1.10") == 0);

    char long_text[kNetworkDiagLineLen + 16];
    memset(long_text, 'x', sizeof(long_text));
    long_text[sizeof(long_text) - 1] = '\0';
    network_diag_line_store(kNetworkDiagOtaLine, long_text);
    NetworkDiagnosticsSnapshot truncated;
    assert(network_diag_snapshot_load(&truncated));
    assert(strlen(truncated.lines[kNetworkDiagOtaLine]) ==
           kNetworkDiagLineLen - 1);

    network_diag_line_store(kNetworkDiagLocalIpLine, "updated");
    assert(strcmp(snapshot.lines[kNetworkDiagLocalIpLine],
                  "本地IP: 192.168.1.10") == 0);

    network_diag_line_store(kNetworkDiagLocalIpLine, nullptr);
    network_diag_state_clear(kNetworkDiagDone);
    assert(network_diag_snapshot_load(&snapshot));
    assert(snapshot.state == kNetworkDiagDone);
    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        assert(snapshot.lines[index][0] == '\0');
    }
    assert(network_diag_state_load() == kNetworkDiagDone);

    std::atomic<bool> start{false};
    std::thread writer([&start]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int iteration = 0; iteration < 1000; ++iteration) {
            network_diag_page_request();
            network_diag_page_clear();
        }
    });
    std::thread reader([&start]() {
        start.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            (void)network_diag_page_requested();
        }
    });
    writer.join();
    reader.join();
    assert(!network_diag_page_requested());

    start.store(false, std::memory_order_release);
    std::thread line_writer([&start]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int iteration = 0; iteration < 1000; ++iteration) {
            network_diag_line_store(kNetworkDiagWeatherLine,
                                    iteration % 2 == 0 ? "weather-ok-a" : "weather-ok-b");
        }
    });
    std::thread snapshot_reader([&start]() {
        start.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            NetworkDiagnosticsSnapshot current = {};
            assert(network_diag_snapshot_load(&current));
            const char *line = current.lines[kNetworkDiagWeatherLine];
            assert(line[0] == '\0' ||
                   strcmp(line, "weather-ok-a") == 0 ||
                   strcmp(line, "weather-ok-b") == 0);
        }
    });
    line_writer.join();
    snapshot_reader.join();

    const char *batch_a[kNetworkDiagLineCount] = {
        "a0:%s", "a1:%s", "a2:%s", "a3:%s", "a4:%s",
        "a5:%s", "a6:%s", "a7:%s", "a8:%s",
    };
    const char *batch_b[kNetworkDiagLineCount] = {
        "b0:%s", "b1:%s", "b2:%s", "b3:%s", "b4:%s",
        "b5:%s", "b6:%s", "b7:%s", "b8:%s",
    };
    assert(network_diag_state_begin(batch_a,
                                    kNetworkDiagLineCount,
                                    "waiting"));
    start.store(false, std::memory_order_release);
    std::thread batch_writer([&start, &batch_a, &batch_b]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int iteration = 0; iteration < 1000; ++iteration) {
            const char *const *formats =
                iteration % 2 == 0 ? batch_a : batch_b;
            assert(network_diag_state_begin(formats,
                                            kNetworkDiagLineCount,
                                            "waiting"));
        }
    });
    std::thread batch_reader([&start]() {
        start.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            NetworkDiagnosticsSnapshot current = {};
            assert(network_diag_snapshot_load(&current));
            const char prefix = current.lines[0][0];
            assert(prefix == 'a' || prefix == 'b' ||
                   current.lines[0][0] == '\0');
            for (int index = 1; index < kNetworkDiagLineCount; ++index) {
                assert(current.lines[index][0] == prefix);
            }
        }
    });
    batch_writer.join();
    batch_reader.join();

    const char *published_a[kNetworkDiagLineCount] = {
        "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8",
    };
    const char *published_b[kNetworkDiagLineCount] = {
        "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8",
    };
    assert(network_diag_state_publish(published_a,
                                      kNetworkDiagLineCount,
                                      kNetworkDiagRunning));
    start.store(false, std::memory_order_release);
    std::thread publish_writer([&start, &published_a, &published_b]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int iteration = 0; iteration < 1000; ++iteration) {
            const bool publish_done = iteration % 2 != 0;
            assert(network_diag_state_publish(
                publish_done ? published_b : published_a,
                kNetworkDiagLineCount,
                publish_done ? kNetworkDiagDone : kNetworkDiagRunning));
        }
    });
    std::thread publish_reader([&start]() {
        start.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            NetworkDiagnosticsSnapshot current = {};
            assert(network_diag_snapshot_load(&current));
            const char prefix = current.lines[0][0];
            assert(prefix == 'a' || prefix == 'b');
            assert(current.state ==
                   (prefix == 'a' ? kNetworkDiagRunning : kNetworkDiagDone));
            for (int index = 1; index < kNetworkDiagLineCount; ++index) {
                assert(current.lines[index][0] == prefix);
            }
        }
    });
    publish_writer.join();
    publish_reader.join();
    return 0;
}
