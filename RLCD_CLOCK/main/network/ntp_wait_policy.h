// 计算 NTP 完成事件的一次性有限等待预算，并避免 Tick 乘法溢出。
#pragma once

template <typename Tick>
constexpr Tick ntp_total_wait_ticks(unsigned attempt_count,
                                    Tick per_attempt_ticks,
                                    Tick max_finite_wait)
{
    if (attempt_count == 0 || per_attempt_ticks == 0 || max_finite_wait == 0) {
        return 0;
    }
    const unsigned long long max_attempts =
        static_cast<unsigned long long>(max_finite_wait / per_attempt_ticks);
    if (static_cast<unsigned long long>(attempt_count) > max_attempts) {
        return max_finite_wait;
    }
    return per_attempt_ticks * static_cast<Tick>(attempt_count);
}

template <typename Tick, typename Milliseconds>
constexpr Milliseconds ntp_wait_ticks_to_milliseconds(
    Tick wait_ticks,
    unsigned tick_rate_hz,
    Milliseconds max_milliseconds)
{
    if (wait_ticks == 0 || tick_rate_hz == 0 || max_milliseconds == 0) {
        return 0;
    }
    const unsigned long long milliseconds =
        static_cast<unsigned long long>(wait_ticks) * 1000ULL /
        static_cast<unsigned long long>(tick_rate_hz);
    if (milliseconds >
        static_cast<unsigned long long>(max_milliseconds)) {
        return max_milliseconds;
    }
    return static_cast<Milliseconds>(milliseconds);
}
