// 实现小智自动返回开关的单一原子所有权边界。
#include "xiaozhi_auto_return_state_internal.h"

#include <atomic>

namespace {
std::atomic<bool> s_xiaozhi_auto_return_enabled{kDefaultXiaozhiAutoReturnEnabled};
}

bool xiaozhi_auto_return_enabled_load()
{
    return s_xiaozhi_auto_return_enabled.load();
}

void xiaozhi_auto_return_enabled_store(bool enabled)
{
    s_xiaozhi_auto_return_enabled.store(enabled);
}
