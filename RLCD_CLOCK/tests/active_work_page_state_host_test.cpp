// 验证当前工作页生产状态模块在并发读写下保持完整整数快照。
#include "active_work_page_state.h"
#include "active_work_page_state_internal.h"
#include "work_page_ids.h"

#include <atomic>
#include <cassert>
#include <thread>

namespace {
constexpr int kIterations = 100000;
}

int main()
{
    assert(active_work_page_load() == 0);
    active_work_page_store(4);
    assert(active_work_page_load() == 4);
    active_work_page_store(-1);
    assert(active_work_page_load() == 4);
    active_work_page_store(kWorkPageCount);
    assert(active_work_page_load() == 4);

    std::atomic<bool> invalid_value_seen{false};
    std::thread writer([] {
        for (int i = 0; i < kIterations; ++i) {
            active_work_page_store(i % kWorkPageCount);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            int page = active_work_page_load();
            if (!is_valid_work_page_id(page)) {
                invalid_value_seen.store(true, std::memory_order_relaxed);
            }
        }
    });
    writer.join();
    reader.join();

    assert(!invalid_value_seen.load(std::memory_order_relaxed));
    active_work_page_store(3);
    assert(active_work_page_load() == 3);
    return 0;
}
