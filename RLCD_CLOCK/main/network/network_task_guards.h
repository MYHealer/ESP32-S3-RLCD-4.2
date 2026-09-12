// 为联网任务提供可重复释放的唤醒锁、HTTP 事务锁和临时超时作用域守卫。
#pragma once

#include "network_http_transaction_lock.h"
#include "power_services.h"

class NetworkAwakeLockGuard {
public:
    NetworkAwakeLockGuard()
        : active_(acquire_network_awake_lock())
    {
    }

    ~NetworkAwakeLockGuard()
    {
        release();
    }

    NetworkAwakeLockGuard(const NetworkAwakeLockGuard &) = delete;
    NetworkAwakeLockGuard &operator=(const NetworkAwakeLockGuard &) = delete;

    bool locked() const
    {
        return active_;
    }

    void release()
    {
        if (active_) {
            release_network_awake_lock();
            active_ = false;
        }
    }

private:
    bool active_ = false;
};

class NetworkHttpTransactionGuard {
public:
    explicit NetworkHttpTransactionGuard(TickType_t timeout)
        : locked_(acquire_network_http_transaction_lock(timeout))
    {
    }

    ~NetworkHttpTransactionGuard()
    {
        if (locked_) {
            release_network_http_transaction_lock();
        }
    }

    bool locked() const
    {
        return locked_;
    }

    NetworkHttpTransactionGuard(const NetworkHttpTransactionGuard &) = delete;
    NetworkHttpTransactionGuard &operator=(const NetworkHttpTransactionGuard &) = delete;

private:
    bool locked_ = false;
};
