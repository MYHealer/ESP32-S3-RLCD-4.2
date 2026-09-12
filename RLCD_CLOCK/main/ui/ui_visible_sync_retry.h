// 提供可见页面按需同步共用的请求、超时、重试和退避状态机。
#pragma once

#include "app_tick_time.h"

template <typename Tick>
class VisibleSyncRetryState {
public:
    void reset_request()
    {
        requested_ = false;
        request_tick_ = 0;
    }

    void reset_attempts()
    {
        attempts_ = 0;
        backoff_active_ = false;
        backoff_started_tick_ = 0;
        backoff_duration_ticks_ = 0;
        backoff_until_tick_ = 0;
    }

    void reset()
    {
        reset_request();
        reset_attempts();
    }

    void cancel_request(Tick now,
                        int max_attempts,
                        Tick backoff_ticks)
    {
        // Cancelling the final in-flight attempt must leave a recovery
        // deadline; attempts == max without backoff would block forever.
        const bool exhausted = requested_ &&
                               max_attempts > 0 &&
                               attempts_ >= max_attempts;
        reset_request();
        if (exhausted) {
            start_backoff(now, backoff_ticks);
        }
    }

    bool request_if_due(Tick now,
                        bool sync_in_flight,
                        bool request_blocked,
                        Tick retry_ticks,
                        int max_attempts,
                        Tick backoff_ticks)
    {
        if (backoff_active_ && app_tick_interval_elapsed(now,
                                                         backoff_started_tick_,
                                                         backoff_duration_ticks_)) {
            reset_attempts();
        }
        bool backoff_active = backoff_active_;
        if (requested_ && !sync_in_flight &&
            app_tick_interval_elapsed(now, request_tick_, retry_ticks)) {
            reset_request();
            if (attempts_ >= max_attempts) {
                start_backoff(now, backoff_ticks);
                backoff_active = true;
            }
        }
        if (requested_ ||
            attempts_ >= max_attempts ||
            backoff_active ||
            request_blocked ||
            sync_in_flight) {
            return false;
        }
        requested_ = true;
        request_tick_ = now;
        ++attempts_;
        return true;
    }

    bool requested() const { return requested_; }
    bool idle() const
    {
        return !requested_ &&
               request_tick_ == 0 &&
               attempts_ == 0 &&
               !backoff_active_ &&
               backoff_started_tick_ == 0 &&
               backoff_duration_ticks_ == 0 &&
               backoff_until_tick_ == 0;
    }
    Tick request_tick() const { return request_tick_; }
    int attempts() const { return attempts_; }
    Tick backoff_until_tick() const { return backoff_until_tick_; }

private:
    void start_backoff(Tick now, Tick backoff_ticks)
    {
        backoff_active_ = true;
        backoff_started_tick_ = now;
        backoff_duration_ticks_ = backoff_ticks;
        backoff_until_tick_ = now + backoff_ticks;
    }

    bool requested_ = false;
    Tick request_tick_ = 0;
    int attempts_ = 0;
    bool backoff_active_ = false;
    Tick backoff_started_tick_ = 0;
    Tick backoff_duration_ticks_ = 0;
    Tick backoff_until_tick_ = 0;
};
