// 提供短时 Tick 间隔和截止时间的回绕安全判断。
#pragma once

#include <type_traits>

template <typename Tick>
constexpr bool app_tick_interval_elapsed(Tick now, Tick started, Tick duration)
{
    static_assert(std::is_integral<Tick>::value && std::is_unsigned<Tick>::value,
                  "Tick must be an unsigned integral type");
    return static_cast<Tick>(now - started) >= duration;
}

// 已确认需要阻塞的正时长换算后至少等待一个 Tick，避免亚 Tick 时长退化为忙等。
template <typename Tick>
constexpr Tick app_tick_nonzero_delay(Tick ticks)
{
    static_assert(std::is_integral<Tick>::value && std::is_unsigned<Tick>::value,
                  "Tick must be an unsigned integral type");
    return ticks == 0 ? static_cast<Tick>(1) : ticks;
}

// 仅用于距离小于 Tick 半周期的短截止时间；当前 UI 截止时间均为秒级。
template <typename Tick>
constexpr bool app_tick_deadline_reached(Tick now, Tick deadline)
{
    static_assert(std::is_integral<Tick>::value && std::is_unsigned<Tick>::value,
                  "Tick must be an unsigned integral type");
    using SignedTick = typename std::make_signed<Tick>::type;
    return static_cast<SignedTick>(static_cast<Tick>(now - deadline)) >= 0;
}

template <typename Tick>
constexpr bool app_tick_deadline_pending(Tick now, Tick deadline)
{
    return !app_tick_deadline_reached(now, deadline);
}

template <typename Tick>
constexpr Tick app_tick_deadline_remaining(Tick now, Tick deadline)
{
    return app_tick_deadline_pending(now, deadline)
               ? static_cast<Tick>(deadline - now)
               : static_cast<Tick>(0);
}

template <typename Tick>
constexpr Tick app_tick_earlier_deadline(Tick now, Tick first, Tick second)
{
    Tick first_remaining = app_tick_deadline_remaining(now, first);
    Tick second_remaining = app_tick_deadline_remaining(now, second);
    return first_remaining < second_remaining ? first : second;
}
