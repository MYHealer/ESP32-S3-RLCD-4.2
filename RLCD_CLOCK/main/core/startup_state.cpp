// 集中维护启动画面生命周期的跨任务原子状态。
#include "startup_state_internal.h"

#include <atomic>

namespace {
std::atomic<bool> s_startup_screen_active{true};
} // namespace

bool startup_screen_active()
{
    return s_startup_screen_active.load(std::memory_order_acquire);
}

void startup_screen_mark_finished()
{
    s_startup_screen_active.store(false, std::memory_order_release);
}
