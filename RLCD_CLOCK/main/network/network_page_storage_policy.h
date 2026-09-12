// 定义工作页 NVS 版本迁移使用的纯计算规则。
#pragma once

#include "work_page_ids.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace network_page_storage {
inline constexpr size_t kLegacyV4WorkPageCount = kWorkPageHistory + 1;
inline constexpr uint8_t kLegacyV4KnownPageMask =
    static_cast<uint8_t>((1U << kLegacyV4WorkPageCount) - 1U);
inline constexpr uint8_t kCurrentKnownPageMask =
    static_cast<uint8_t>((1U << kWorkPageCount) - 1U);

constexpr uint8_t migrate_v4_page_mask(uint8_t legacy_mask)
{
    return static_cast<uint8_t>(legacy_mask | (1U << kWorkPageXiaozhiAI));
}

inline bool migrate_v4_page_order(const uint8_t *legacy_order,
                                  size_t legacy_order_size,
                                  uint8_t *current_order,
                                  size_t current_order_size)
{
    if (!legacy_order || legacy_order_size != kLegacyV4WorkPageCount ||
        !current_order || current_order_size != kWorkPageCount) {
        return false;
    }
    memcpy(current_order, legacy_order, legacy_order_size);
    current_order[kWorkPageXiaozhiAI] = kWorkPageXiaozhiAI;
    current_order[kWorkPageAggregateClock] = kWorkPageAggregateClock;
    return true;
}

static_assert(kWorkPageXiaozhiAI == static_cast<int>(kLegacyV4WorkPageCount),
              "v5 migration expects Xiaozhi AI after every v4 page");
static_assert(kWorkPageCount == kWorkPageAggregateClock + 1,
              "v6 migration expects aggregate clock to be the final page");
static_assert((kLegacyV4KnownPageMask & (1U << kWorkPageXiaozhiAI)) == 0,
              "v4 mask must not contain the Xiaozhi AI page");
} // namespace network_page_storage
