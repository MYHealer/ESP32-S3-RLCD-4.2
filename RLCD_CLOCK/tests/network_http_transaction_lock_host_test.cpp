// 直接验证 HTTP/WSS 全局事务锁生产实现的初始化、争用和释放语义。
#include "network_http_transaction_lock_internal.h"

#include <assert.h>
#include <atomic>
#include <thread>

bool g_mutex_create_fail = false;
int g_mutex_create_calls = 0;

extern const char *const TAG = "HttpLockHost";

int main()
{
    assert(!acquire_network_http_transaction_lock(0));
    release_network_http_transaction_lock();

    g_mutex_create_fail = true;
    assert(!init_network_http_transaction_lock());
    assert(g_mutex_create_calls == 1);

    g_mutex_create_fail = false;
    assert(init_network_http_transaction_lock());
    assert(init_network_http_transaction_lock());
    assert(g_mutex_create_calls == 2);

    assert(acquire_network_http_transaction_lock(0));
    std::atomic<bool> acquired_while_busy{true};
    std::thread contender([&]() {
        acquired_while_busy.store(acquire_network_http_transaction_lock(0));
    });
    contender.join();
    assert(!acquired_while_busy.load());
    release_network_http_transaction_lock();

    std::atomic<bool> acquired_after_release{false};
    std::thread successor([&]() {
        bool acquired = acquire_network_http_transaction_lock(0);
        acquired_after_release.store(acquired);
        if (acquired) {
            release_network_http_transaction_lock();
        }
    });
    successor.join();
    assert(acquired_after_release.load());
    return 0;
}
