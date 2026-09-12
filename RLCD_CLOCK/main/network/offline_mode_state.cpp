// 使用私有原子值发布离线模式运行态，避免跨模块直接访问业务全局量。
#include "offline_mode_state_internal.h"

#include <atomic>

namespace {
std::atomic<bool> s_offline_mode_enabled{false};
} // namespace

bool offline_mode_enabled_load()
{
    return s_offline_mode_enabled.load(std::memory_order_acquire);
}

void offline_mode_enabled_store(bool enabled)
{
    s_offline_mode_enabled.store(enabled, std::memory_order_release);
}
