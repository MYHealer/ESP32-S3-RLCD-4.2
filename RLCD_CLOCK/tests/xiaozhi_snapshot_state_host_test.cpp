// 直接验证生产快照存储只在最终内容变化时通知 UI。
#include "xiaozhi_snapshot_state.h"

#include "freertos/semphr.h"
#include "ui_task_notify.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace {
int s_notify_count = 0;
int s_mutex_token = 0;
int s_mutex_create_count = 0;
int s_mutex_delete_count = 0;
int s_mutex_take_count = 0;
int s_mutex_give_count = 0;
bool s_mutex_held = false;
bool s_fail_mutex_take = false;

void expect_mutex_released()
{
    assert(!s_mutex_held);
    assert(s_mutex_take_count == s_mutex_give_count);
}
} // namespace

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    assert(storage != nullptr);
    ++s_mutex_create_count;
    return &s_mutex_token;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t)
{
    if (s_fail_mutex_take) {
        return pdFALSE;
    }
    assert(!s_mutex_held);
    s_mutex_held = true;
    ++s_mutex_take_count;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t)
{
    assert(s_mutex_held);
    s_mutex_held = false;
    ++s_mutex_give_count;
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t)
{
    assert(!s_mutex_held);
    ++s_mutex_delete_count;
}

void notify_ui_task()
{
    // UI 通知不得发生在快照 mutex 内，避免通知路径反向读取快照时死锁。
    assert(!s_mutex_held);
    ++s_notify_count;
}

void register_ui_task_handle(TaskHandle_t)
{
}

int main()
{
    XiaozhiActivitySnapshot activity = xiaozhi_snapshot_activity_load();
    assert(activity.state == kXiaozhiAiInactive);
    assert(activity.activity_sequence == 0);

    XiaozhiAiSnapshot unavailable = {};
    xiaozhi_snapshot_get(&unavailable);
    assert(unavailable.state == kXiaozhiAiInactive);
    assert(strcmp(unavailable.status, kXiaozhiDefaultStatus) == 0);
    uint32_t snapshot_version = 0;
    XiaozhiAiSnapshot cached_snapshot = {};
    assert(!xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                             &cached_snapshot));
    assert(snapshot_version == 0);

    assert(xiaozhi_snapshot_state_init());
    assert(s_mutex_create_count == 1);
    assert(xiaozhi_snapshot_state_init());
    assert(s_mutex_create_count == 1);
    assert(xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                            &cached_snapshot));
    assert(snapshot_version != 0);
    assert(cached_snapshot.state == kXiaozhiAiInactive);
    const int take_count_after_initial_snapshot = s_mutex_take_count;
    assert(!xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                             &cached_snapshot));
    assert(s_mutex_take_count == take_count_after_initial_snapshot);

    xiaozhi_snapshot_set(kXiaozhiAiReady, "等待唤醒词", "请说你好，小智", "123456");
    assert(s_notify_count == 1);
    assert(xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                            &cached_snapshot));
    assert(cached_snapshot.state == kXiaozhiAiReady);
    assert(strcmp(cached_snapshot.detail, "请说你好，小智") == 0);
    activity = xiaozhi_snapshot_activity_load();
    assert(activity.state == kXiaozhiAiReady);
    assert(activity.activity_sequence == 0);
    expect_mutex_released();
    xiaozhi_snapshot_set(kXiaozhiAiReady, "等待唤醒词", "请说你好，小智", "123456");
    assert(s_notify_count == 1);
    assert(!xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                             &cached_snapshot));
    xiaozhi_snapshot_set_status_preserving_detail(kXiaozhiAiReady, "等待唤醒词");
    assert(s_notify_count == 1);

    xiaozhi_snapshot_set_emotion("happy");
    assert(s_notify_count == 2);
    xiaozhi_snapshot_set_emotion("happy");
    assert(s_notify_count == 2);

    // 非 Speaking 状态的完整写入仍执行原有情绪复位规则。
    xiaozhi_snapshot_set(kXiaozhiAiReady, "等待唤醒词", "请说你好，小智", "123456");
    assert(s_notify_count == 3);
    xiaozhi_snapshot_set(kXiaozhiAiReady, "等待唤醒词", "请说你好，小智", "123456");
    assert(s_notify_count == 3);

    xiaozhi_snapshot_mark_user_activity();
    xiaozhi_snapshot_mark_user_activity();
    assert(s_notify_count == 5);
    activity = xiaozhi_snapshot_activity_load();
    assert(activity.state == kXiaozhiAiReady);
    assert(activity.activity_sequence == 2);
    assert(xiaozhi_snapshot_get_if_changed(&snapshot_version,
                                            &cached_snapshot));
    assert(cached_snapshot.activity_sequence == 2);

    XiaozhiAiSnapshot snapshot = {};
    xiaozhi_snapshot_get(&snapshot);
    assert(snapshot.state == kXiaozhiAiReady);
    assert(strcmp(snapshot.emotion, "neutral") == 0);
    assert(snapshot.waveform_level == 0);
    assert(snapshot.activity_sequence == 2);
    expect_mutex_released();

    const int notify_before_take_failure = s_notify_count;
    uint32_t stale_snapshot_version = 0;
    s_fail_mutex_take = true;
    xiaozhi_snapshot_set(kXiaozhiAiError, "error", "ignored");
    xiaozhi_snapshot_set_status_preserving_detail(kXiaozhiAiError, "error");
    xiaozhi_snapshot_set_emotion("sad");
    xiaozhi_snapshot_mark_user_activity();
    XiaozhiAiSnapshot failed_read = {};
    xiaozhi_snapshot_get(&failed_read);
    assert(!xiaozhi_snapshot_get_if_changed(&stale_snapshot_version,
                                             &cached_snapshot));
    s_fail_mutex_take = false;
    assert(s_notify_count == notify_before_take_failure);
    assert(failed_read.state == kXiaozhiAiInactive);
    assert(strcmp(failed_read.status, kXiaozhiDefaultStatus) == 0);
    assert(stale_snapshot_version == 0);
    assert(xiaozhi_snapshot_get_if_changed(&stale_snapshot_version,
                                            &cached_snapshot));
    assert(stale_snapshot_version == snapshot_version);
    assert(cached_snapshot.activity_sequence == 2);
    activity = xiaozhi_snapshot_activity_load();
    assert(activity.state == kXiaozhiAiReady);
    assert(activity.activity_sequence == 2);
    expect_mutex_released();

    xiaozhi_snapshot_get(&snapshot);
    assert(snapshot.state == kXiaozhiAiReady);
    assert(strcmp(snapshot.emotion, "neutral") == 0);
    assert(snapshot.activity_sequence == 2);
    expect_mutex_released();

    xiaozhi_snapshot_state_deinit();
    assert(s_mutex_delete_count == 1);
    xiaozhi_snapshot_set(kXiaozhiAiError, "error", "ignored");
    assert(s_notify_count == 5);

    assert(xiaozhi_snapshot_state_init());
    assert(s_mutex_create_count == 2);
    xiaozhi_snapshot_state_deinit();
    assert(s_mutex_delete_count == 2);
    puts("Xiaozhi snapshot state host tests passed");
    return 0;
}
