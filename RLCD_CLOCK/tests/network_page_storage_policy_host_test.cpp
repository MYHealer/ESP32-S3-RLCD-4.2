// 验证工作页 v4 到 v5 的 mask 与顺序迁移规则。
#include "network_page_storage_policy.h"

#include <cassert>
#include <cstring>

int main()
{
    static_assert(network_page_storage::kLegacyV4KnownPageMask == 0x3f);
    static_assert(network_page_storage::kCurrentKnownPageMask == 0xff);
    static_assert(network_page_storage::migrate_v4_page_mask(0x00) == 0x40);
    static_assert(network_page_storage::migrate_v4_page_mask(0x3f) == 0x7f);

    const uint8_t legacy_order[] = {2, 0, 1, 3, 4, 5};
    const uint8_t expected_order[] = {2, 0, 1, 3, 4, 5, 6, 7};
    uint8_t current_order[kWorkPageCount] = {};
    assert(network_page_storage::migrate_v4_page_order(legacy_order,
                                                       sizeof(legacy_order),
                                                       current_order,
                                                       sizeof(current_order)));
    assert(memcmp(current_order, expected_order, sizeof(expected_order)) == 0);

    uint8_t untouched[kWorkPageCount] = {9, 9, 9, 9, 9, 9, 9, 9};
    assert(!network_page_storage::migrate_v4_page_order(nullptr,
                                                        sizeof(legacy_order),
                                                        untouched,
                                                        sizeof(untouched)));
    assert(!network_page_storage::migrate_v4_page_order(legacy_order,
                                                        sizeof(legacy_order) - 1,
                                                        untouched,
                                                        sizeof(untouched)));
    assert(!network_page_storage::migrate_v4_page_order(legacy_order,
                                                        sizeof(legacy_order),
                                                        untouched,
                                                        sizeof(untouched) - 1));
    for (uint8_t value : untouched) {
        assert(value == 9);
    }
    return 0;
}
