// 验证任务 semaphore 作用域锁的成功、失败、空句柄和自动释放语义。
#include "scoped_semaphore_lock.h"

#include <assert.h>

int main()
{
    StaticTaskMutex owned;
    assert(owned.handle() == nullptr);
    test_mutex_create_allowed = false;
    assert(!owned.init());
    assert(owned.handle() == nullptr);
    assert(test_mutex_create_count == 1);
    test_mutex_create_allowed = true;
    assert(owned.init());
    assert(owned.handle() != nullptr);
    assert(test_mutex_create_count == 2);
    assert(owned.init());
    assert(test_mutex_create_count == 2);
    owned.handle()->take_allowed = true;
    {
        ScopedSemaphoreLock lock(owned, 9);
        assert(lock);
        assert(owned.handle()->last_wait_ticks == 9);
    }
    assert(owned.handle()->give_count == 1);

    TestSemaphore available = {.take_allowed = true};
    {
        ScopedSemaphoreLock lock(&available, 17);
        assert(lock);
        assert(available.take_count == 1);
        assert(available.give_count == 0);
        assert(available.last_wait_ticks == 17);
    }
    assert(available.give_count == 1);

    TestSemaphore unavailable = {.take_allowed = false};
    {
        ScopedSemaphoreLock lock(&unavailable);
        assert(!lock);
        assert(unavailable.take_count == 1);
        assert(unavailable.last_wait_ticks == portMAX_DELAY);
    }
    assert(unavailable.give_count == 0);

    ScopedSemaphoreLock empty(nullptr);
    assert(!empty);
    return 0;
}
