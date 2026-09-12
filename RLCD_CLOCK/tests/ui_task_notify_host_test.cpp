// 验证 UI 任务通知在句柄发布前安全跳过并在发布后投递。
#include "ui_task_notify.h"

#include <cassert>
#include <cstdio>

namespace {
int s_notify_calls = 0;
TaskHandle_t s_last_notified_handle = nullptr;
} // namespace

void xTaskNotifyGive(TaskHandle_t handle)
{
    ++s_notify_calls;
    s_last_notified_handle = handle;
}

int main()
{
    notify_ui_task();
    assert(s_notify_calls == 0);

    TaskHandle_t first = reinterpret_cast<TaskHandle_t>(0x1);
    register_ui_task_handle(first);
    notify_ui_task();
    assert(s_notify_calls == 1);
    assert(s_last_notified_handle == first);

    register_ui_task_handle(nullptr);
    notify_ui_task();
    assert(s_notify_calls == 1);

    TaskHandle_t second = reinterpret_cast<TaskHandle_t>(0x2);
    register_ui_task_handle(second);
    notify_ui_task();
    assert(s_notify_calls == 2);
    assert(s_last_notified_handle == second);
    std::puts("UI task notify host tests passed");
    return 0;
}
