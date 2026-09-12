// 实现小智状态快照的单一互斥访问边界。
#include "xiaozhi_snapshot_state.h"

#include "scoped_semaphore_lock.h"
#include "ui_task_notify.h"
#include "xiaozhi_snapshot_change.h"
#include "xiaozhi_text_utils.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <string.h>

namespace {
constexpr const char *kNeutralEmotion = "neutral";
constexpr uint32_t kActivityStateBits = 3;
constexpr uint32_t kActivityStateMask = (1U << kActivityStateBits) - 1U;

StaticSemaphore_t s_snapshot_mutex_storage = {};
SemaphoreHandle_t s_snapshot_mutex = nullptr;
// 固定 char 数组的静态聚合初始化需要字符串字面量。
XiaozhiAiSnapshot s_snapshot = {
    kXiaozhiAiInactive, "小智准备中", "", "", "neutral", 0, 0};
std::atomic<uint32_t> s_activity_snapshot{
    static_cast<uint32_t>(kXiaozhiAiInactive)};
std::atomic<uint32_t> s_snapshot_version{1};

static_assert(static_cast<uint32_t>(kXiaozhiAiError) <= kActivityStateMask,
              "Xiaozhi states must fit the activity snapshot state field");

constexpr uint32_t pack_activity_snapshot(XiaozhiAiState state,
                                          uint32_t activity_sequence)
{
    return (activity_sequence << kActivityStateBits) |
           (static_cast<uint32_t>(state) & kActivityStateMask);
}

void publish_activity_snapshot_locked()
{
    s_activity_snapshot.store(pack_activity_snapshot(s_snapshot.state,
                                                     s_snapshot.activity_sequence),
                              std::memory_order_release);
}

void publish_snapshot_version_locked()
{
    s_snapshot_version.fetch_add(1, std::memory_order_release);
}

void begin_state_update_locked(XiaozhiAiState state, const char *status)
{
    s_snapshot.state = state;
    xiaozhi_protocol::utf8_safe_copy(s_snapshot.status,
                                     sizeof(s_snapshot.status),
                                     status);
}

void finish_state_update_locked(XiaozhiAiState state)
{
    if (state != kXiaozhiAiSpeaking) {
        strlcpy(s_snapshot.emotion, kNeutralEmotion, sizeof(s_snapshot.emotion));
    }
    s_snapshot.waveform_level =
        state == kXiaozhiAiListening || state == kXiaozhiAiSpeaking ? 1 : 0;
    publish_activity_snapshot_locked();
}
} // namespace

bool xiaozhi_snapshot_state_init()
{
    if (s_snapshot_mutex) {
        return true;
    }
    s_snapshot_mutex = xSemaphoreCreateMutexStatic(&s_snapshot_mutex_storage);
    return s_snapshot_mutex != nullptr;
}

void xiaozhi_snapshot_state_deinit()
{
    if (!s_snapshot_mutex) {
        return;
    }
    vSemaphoreDelete(s_snapshot_mutex);
    s_snapshot_mutex = nullptr;
}

void xiaozhi_snapshot_set(XiaozhiAiState state,
                          const char *status,
                          const char *detail,
                          const char *binding_code)
{
    bool changed = false;
    {
        ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(100));
        if (!state_lock) {
            return;
        }
        const XiaozhiAiSnapshot before = s_snapshot;
        begin_state_update_locked(state, status);
        xiaozhi_protocol::utf8_safe_copy(s_snapshot.detail,
                                         sizeof(s_snapshot.detail),
                                         detail);
        xiaozhi_protocol::utf8_safe_copy(s_snapshot.binding_code,
                                         sizeof(s_snapshot.binding_code),
                                         binding_code);
        finish_state_update_locked(state);
        changed = !xiaozhi_snapshot_content_equal(before, s_snapshot);
        if (changed) {
            publish_snapshot_version_locked();
        }
    }
    if (changed) {
        notify_ui_task();
    }
}

void xiaozhi_snapshot_set_status_preserving_detail(XiaozhiAiState state,
                                                    const char *status)
{
    bool changed = false;
    {
        ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(100));
        if (!state_lock) {
            return;
        }
        const XiaozhiAiSnapshot before = s_snapshot;
        begin_state_update_locked(state, status);
        finish_state_update_locked(state);
        changed = !xiaozhi_snapshot_content_equal(before, s_snapshot);
        if (changed) {
            publish_snapshot_version_locked();
        }
    }
    if (changed) {
        notify_ui_task();
    }
}

void xiaozhi_snapshot_set_emotion(const char *emotion)
{
    if (!emotion) {
        return;
    }
    bool changed = false;
    {
        ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(100));
        if (!state_lock) {
            return;
        }
        const XiaozhiAiSnapshot before = s_snapshot;
        xiaozhi_protocol::utf8_safe_copy(s_snapshot.emotion,
                                         sizeof(s_snapshot.emotion),
                                         emotion);
        changed = !xiaozhi_snapshot_content_equal(before, s_snapshot);
        if (changed) {
            publish_snapshot_version_locked();
        }
    }
    if (changed) {
        notify_ui_task();
    }
}

void xiaozhi_snapshot_mark_user_activity()
{
    {
        ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(100));
        if (!state_lock) {
            return;
        }
        ++s_snapshot.activity_sequence;
        publish_activity_snapshot_locked();
        publish_snapshot_version_locked();
    }
    notify_ui_task();
}

void xiaozhi_snapshot_get(XiaozhiAiSnapshot *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(50));
    if (!state_lock) {
        out->state = kXiaozhiAiInactive;
        strlcpy(out->status, kXiaozhiDefaultStatus, sizeof(out->status));
        return;
    }
    *out = s_snapshot;
}

bool xiaozhi_snapshot_get_if_changed(uint32_t *version,
                                     XiaozhiAiSnapshot *out)
{
    if (!version || !out) {
        return false;
    }
    uint32_t current_version =
        s_snapshot_version.load(std::memory_order_acquire);
    if (*version == current_version) {
        return false;
    }
    ScopedSemaphoreLock state_lock(s_snapshot_mutex, pdMS_TO_TICKS(50));
    if (!state_lock) {
        return false;
    }
    current_version = s_snapshot_version.load(std::memory_order_relaxed);
    if (*version == current_version) {
        return false;
    }
    *out = s_snapshot;
    *version = current_version;
    return true;
}

XiaozhiActivitySnapshot xiaozhi_snapshot_activity_load()
{
    const uint32_t packed = s_activity_snapshot.load(std::memory_order_acquire);
    return {
        static_cast<XiaozhiAiState>(packed & kActivityStateMask),
        packed >> kActivityStateBits,
    };
}
