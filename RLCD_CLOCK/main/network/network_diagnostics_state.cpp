// 管理网络检测后台写入与 UI 读取之间的一致状态快照。
#include "network_diagnostics_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <esp_attr.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
StaticTaskMutex s_network_diag_mutex;
NetworkDiagState s_network_diag_state = kNetworkDiagIdle;
EXT_RAM_BSS_ATTR char s_network_diag_lines[kNetworkDiagLineCount]
                                           [kNetworkDiagLineLen] = {};
std::atomic<bool> s_network_diag_page_requested{false};
uint32_t s_network_diag_page_revision = 0;

void advance_network_diag_page_revision()
{
    ++s_network_diag_page_revision;
    if (s_network_diag_page_revision == 0) {
        ++s_network_diag_page_revision;
    }
}

void clear_network_diag_page_locked()
{
    s_network_diag_page_requested.store(false, std::memory_order_release);
    advance_network_diag_page_revision();
}

void copy_line(char *out, const char *text)
{
    memset(out, 0, kNetworkDiagLineLen);
    if (!text) {
        return;
    }
    size_t length = strnlen(text, kNetworkDiagLineLen - 1);
    memcpy(out, text, length);
}
} // namespace

bool network_diagnostics_state_init()
{
    return s_network_diag_mutex.init();
}

void network_diag_state_clear(NetworkDiagState state)
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return;
    }
    s_network_diag_state = state;
    memset(s_network_diag_lines, 0, sizeof(s_network_diag_lines));
}

bool network_diag_state_begin(const char *const *line_formats,
                              size_t format_count,
                              const char *status_text)
{
    if (!line_formats || format_count != kNetworkDiagLineCount ||
        !status_text) {
        return false;
    }
    for (size_t index = 0; index < format_count; ++index) {
        if (!line_formats[index]) {
            return false;
        }
    }
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return false;
    }
    for (size_t index = 0; index < format_count; ++index) {
        int written = snprintf(s_network_diag_lines[index],
                               kNetworkDiagLineLen,
                               line_formats[index],
                               status_text);
        if (written < 0) {
            s_network_diag_lines[index][0] = '\0';
        } else {
            s_network_diag_lines[index][kNetworkDiagLineLen - 1] = '\0';
        }
    }
    s_network_diag_state = kNetworkDiagRunning;
    return true;
}

bool network_diag_state_publish(const char *const *lines,
                                size_t line_count,
                                NetworkDiagState state)
{
    if (!lines || line_count != kNetworkDiagLineCount) {
        return false;
    }
    for (size_t index = 0; index < line_count; ++index) {
        if (!lines[index]) {
            return false;
        }
    }
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return false;
    }
    for (size_t index = 0; index < line_count; ++index) {
        copy_line(s_network_diag_lines[index], lines[index]);
    }
    s_network_diag_state = state;
    return true;
}

void network_diag_state_store(NetworkDiagState state)
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return;
    }
    s_network_diag_state = state;
}

NetworkDiagState network_diag_state_load()
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return kNetworkDiagIdle;
    }
    return s_network_diag_state;
}

void network_diag_line_store(int index, const char *text)
{
    if (!network_diag_line_index_valid(index)) {
        return;
    }
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return;
    }
    copy_line(s_network_diag_lines[index], text);
}

bool network_diag_snapshot_load(NetworkDiagnosticsSnapshot *snapshot)
{
    if (!snapshot) {
        return false;
    }
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return false;
    }
    snapshot->state = s_network_diag_state;
    memcpy(snapshot->lines, s_network_diag_lines, sizeof(snapshot->lines));
    return true;
}

bool network_diag_page_requested()
{
    return s_network_diag_page_requested.load(std::memory_order_acquire);
}

NetworkDiagPageRequestSnapshot network_diag_page_snapshot_load()
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return {};
    }
    return {
        s_network_diag_page_requested.load(std::memory_order_relaxed),
        s_network_diag_page_revision,
    };
}

void network_diag_page_request()
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock) {
        return;
    }
    s_network_diag_page_requested.store(true, std::memory_order_release);
    advance_network_diag_page_revision();
}

void network_diag_page_clear()
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock ||
        !s_network_diag_page_requested.load(std::memory_order_relaxed)) {
        return;
    }
    clear_network_diag_page_locked();
}

bool network_diag_page_clear_if_current(
    const NetworkDiagPageRequestSnapshot &snapshot)
{
    ScopedSemaphoreLock lock(s_network_diag_mutex);
    if (!lock ||
        !snapshot.requested ||
        !s_network_diag_page_requested.load(std::memory_order_relaxed) ||
        s_network_diag_page_revision != snapshot.revision) {
        return false;
    }
    clear_network_diag_page_locked();
    return true;
}
