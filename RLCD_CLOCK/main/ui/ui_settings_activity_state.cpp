// 集中管理设置页跨任务活动状态，避免输入、UI 和 OTA 间的数据竞争。
#include "ui_settings_activity_state_internal.h"

#include "scoped_semaphore_lock.h"

#include <atomic>

namespace {
std::atomic<bool> s_page_requested{false};
std::atomic<uint32_t> s_action_sequence{0};
std::atomic<uint32_t> s_last_activity_tick{0};
std::atomic<uint32_t> s_activity_revision{0};
StaticTaskMutex s_activity_mutex;
}

bool settings_activity_state_init()
{
    return s_activity_mutex.init();
}

bool settings_page_requested()
{
    return s_page_requested.load(std::memory_order_acquire);
}

void settings_page_request()
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return;
    }
    s_page_requested.store(true, std::memory_order_release);
}

void settings_page_clear()
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return;
    }
    s_page_requested.store(false, std::memory_order_release);
}

uint32_t settings_activity_action_sequence()
{
    return s_action_sequence.load(std::memory_order_acquire);
}

uint32_t settings_activity_last_tick()
{
    return s_last_activity_tick.load(std::memory_order_acquire);
}

SettingsActivitySnapshot settings_activity_snapshot()
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return {};
    }
    SettingsActivitySnapshot snapshot;
    snapshot.action_sequence =
        s_action_sequence.load(std::memory_order_relaxed);
    snapshot.last_activity_tick =
        s_last_activity_tick.load(std::memory_order_relaxed);
    snapshot.revision =
        s_activity_revision.load(std::memory_order_relaxed);
    return snapshot;
}

bool settings_activity_claim_if_current(
    const SettingsActivitySnapshot &snapshot)
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return false;
    }
    if (s_activity_revision.load(std::memory_order_relaxed) !=
        snapshot.revision) {
        return false;
    }
    s_activity_revision.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool settings_page_clear_if_activity_current(
    const SettingsActivitySnapshot &snapshot)
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return false;
    }
    if (!s_page_requested.load(std::memory_order_relaxed) ||
        s_activity_revision.load(std::memory_order_relaxed) !=
            snapshot.revision) {
        return false;
    }
    s_page_requested.store(false, std::memory_order_release);
    return true;
}

void settings_activity_record(uint32_t tick)
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return;
    }
    s_last_activity_tick.store(tick, std::memory_order_relaxed);
    s_activity_revision.fetch_add(1, std::memory_order_relaxed);
}

void settings_activity_record_action(uint32_t tick)
{
    ScopedSemaphoreLock lock(s_activity_mutex);
    if (!lock) {
        return;
    }
    s_last_activity_tick.store(tick, std::memory_order_relaxed);
    s_action_sequence.fetch_add(1, std::memory_order_relaxed);
    s_activity_revision.fetch_add(1, std::memory_order_relaxed);
}
