// 集中管理当前工作页，避免 UI、输入和后台任务间的数据竞争。
#include "active_work_page_state_internal.h"

#include "work_page_ids.h"

#include <atomic>

namespace {
std::atomic<int> s_active_work_page{0};
}

int active_work_page_load()
{
    return s_active_work_page.load(std::memory_order_acquire);
}

void active_work_page_store(int page)
{
    if (!is_valid_work_page_id(page)) {
        return;
    }
    s_active_work_page.store(page, std::memory_order_release);
}
