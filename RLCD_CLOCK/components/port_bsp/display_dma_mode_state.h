// 使用单个原子位图维护显示 OTA 静默与 DMA 保守模式嵌套深度。
#pragma once

#include <atomic>
#include <stdint.h>

inline constexpr uint32_t kDisplayDmaConservativeMaxDepth = 0x7fffffffU;

class DisplayDmaModeState {
public:
    bool conservative_mode() const
    {
        return state_.load(std::memory_order_acquire) != 0;
    }

    void set_ota_quiet(bool enabled)
    {
        if (enabled) {
            state_.fetch_or(kOtaQuietMask, std::memory_order_release);
        } else {
            state_.fetch_and(~kOtaQuietMask, std::memory_order_release);
        }
    }

    void acquire_conservative_mode()
    {
        uint32_t current = state_.load(std::memory_order_relaxed);
        while ((current & kDepthMask) < kDisplayDmaConservativeMaxDepth) {
            const uint32_t next = current + 1;
            if (state_.compare_exchange_weak(current,
                                             next,
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
                return;
            }
        }
    }

    void release_conservative_mode()
    {
        uint32_t current = state_.load(std::memory_order_relaxed);
        while ((current & kDepthMask) > 0) {
            const uint32_t next = current - 1;
            if (state_.compare_exchange_weak(current,
                                             next,
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
                return;
            }
        }
    }

private:
    static constexpr uint32_t kDepthMask = 0x7fffffffU;
    static constexpr uint32_t kOtaQuietMask = 0x80000000U;
    static_assert(kDisplayDmaConservativeMaxDepth <= kDepthMask,
                  "display DMA conservative depth must fit atomic state");
    static_assert((kDepthMask & kOtaQuietMask) == 0,
                  "display DMA depth and OTA quiet bits must not overlap");

    std::atomic<uint32_t> state_{0};
};
