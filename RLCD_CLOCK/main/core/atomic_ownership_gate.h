// 提供跨任务单所有者资源及异步任务生命周期的轻量原子门控。
#pragma once

#include <atomic>
#include <cstdint>

class AtomicOwnershipGate {
public:
    bool try_acquire()
    {
        bool expected = false;
        return active_.compare_exchange_strong(expected,
                                               true,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
    }

    void release()
    {
        active_.store(false, std::memory_order_release);
    }

    bool active() const
    {
        return active_.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> active_{false};
};

enum class AtomicTaskStartClaim : uint8_t {
    Claimed,
    AlreadyActive,
    Stopping,
};

class AtomicTaskLifecycleGate {
public:
    AtomicTaskStartClaim try_begin_start()
    {
        State current = state_.load(std::memory_order_acquire);
        for (;;) {
            if (current == State::Starting || current == State::Running) {
                return AtomicTaskStartClaim::AlreadyActive;
            }
            if (current == State::StopRequested) {
                return AtomicTaskStartClaim::Stopping;
            }
            if (state_.compare_exchange_weak(current,
                                             State::Starting,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
                return AtomicTaskStartClaim::Claimed;
            }
        }
    }

    void mark_running()
    {
        State expected = State::Starting;
        (void)state_.compare_exchange_strong(expected,
                                             State::Running,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
    }

    void mark_stopped()
    {
        state_.store(State::Stopped, std::memory_order_release);
    }

    bool request_stop()
    {
        State current = state_.load(std::memory_order_acquire);
        for (;;) {
            if (current == State::Stopped) {
                return false;
            }
            if (current == State::StopRequested) {
                return true;
            }
            if (state_.compare_exchange_weak(current,
                                             State::StopRequested,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool stop_requested() const
    {
        return state_.load(std::memory_order_acquire) == State::StopRequested;
    }

    bool active() const
    {
        return state_.load(std::memory_order_acquire) != State::Stopped;
    }

private:
    enum class State : uint8_t {
        Stopped,
        Starting,
        Running,
        StopRequested,
    };

    std::atomic<State> state_{State::Stopped};
};
