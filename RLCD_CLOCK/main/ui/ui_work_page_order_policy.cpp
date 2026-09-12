// 实现不依赖任务、锁或 UI 对象的工作页顺序纯数组策略。
#include "ui_work_page_order_policy.h"

#include "work_page_ids.h"

#include <string.h>

namespace work_page_order_policy {
namespace {

constexpr uint8_t page_mask(int page)
{
    return static_cast<uint8_t>(1U << page);
}

bool order_buffer_is_valid(const uint8_t *order, size_t order_size)
{
    return order && order_size == kWorkPageCount;
}

} // namespace

bool is_work_page(int page)
{
    return is_valid_work_page_id(page);
}

bool is_order_index(int index)
{
    return index >= 0 && index < kWorkPageCount;
}

bool index_found(int index)
{
    return index != kInvalidIndex;
}

bool order_is_valid(const uint8_t *order, size_t order_size)
{
    if (!order_buffer_is_valid(order, order_size)) {
        return false;
    }
    bool seen[kWorkPageCount] = {};
    for (int i = 0; i < kWorkPageCount; ++i) {
        uint8_t page = order[i];
        if (!is_valid_work_page_id(page) || seen[page]) {
            return false;
        }
        seen[page] = true;
    }
    for (bool present : seen) {
        if (!present) {
            return false;
        }
    }
    return true;
}

bool page_is_enabled(int page, uint8_t page_mask_value)
{
    return is_work_page(page) &&
           (page_mask_value & page_mask(page)) != 0;
}

bool mask_has_valid_home(uint8_t page_mask_value)
{
    for (int page = kWorkPageWeatherClock; page < kWorkPageCount; ++page) {
        if (page != kWorkPageXiaozhiAI &&
            (page_mask_value & page_mask(page)) != 0) {
            return true;
        }
    }
    return false;
}

bool order_has_valid_home(const uint8_t *order,
                          size_t order_size,
                          uint8_t page_mask_value)
{
    if (!order_is_valid(order, order_size)) {
        return false;
    }
    const int first_enabled =
        first_enabled_index(order, order_size, page_mask_value);
    return index_found(first_enabled) &&
           order[first_enabled] != kWorkPageXiaozhiAI;
}

bool swap_entries_preserving_home(uint8_t *order,
                                  size_t order_size,
                                  uint8_t page_mask_value,
                                  int first_index,
                                  int second_index)
{
    if (!order_is_valid(order, order_size) ||
        !is_order_index(first_index) ||
        !is_order_index(second_index)) {
        return false;
    }
    const uint8_t first_page = order[first_index];
    order[first_index] = order[second_index];
    order[second_index] = first_page;
    if (order_has_valid_home(order, order_size, page_mask_value)) {
        return true;
    }
    order[second_index] = order[first_index];
    order[first_index] = first_page;
    return false;
}

int index_of(const uint8_t *order, size_t order_size, int page)
{
    if (!order_buffer_is_valid(order, order_size)) {
        return kInvalidIndex;
    }
    for (int i = 0; i < kWorkPageCount; ++i) {
        if (order[i] == page) {
            return i;
        }
    }
    return kInvalidIndex;
}

int first_enabled_index(const uint8_t *order,
                        size_t order_size,
                        uint8_t page_mask_value)
{
    if (!order_buffer_is_valid(order, order_size)) {
        return kInvalidIndex;
    }
    for (int i = 0; i < kWorkPageCount; ++i) {
        if (page_is_enabled(order[i], page_mask_value)) {
            return i;
        }
    }
    return kInvalidIndex;
}

int next_enabled_index(const uint8_t *order,
                       size_t order_size,
                       uint8_t page_mask_value,
                       int current_order_index)
{
    if (!order_buffer_is_valid(order, order_size)) {
        return kInvalidIndex;
    }
    for (int step = 1; step <= kWorkPageCount; ++step) {
        int index = (current_order_index + step + kWorkPageCount) % kWorkPageCount;
        if (page_is_enabled(order[index], page_mask_value)) {
            return index;
        }
    }
    return kInvalidIndex;
}

int valid_enabled_index(const uint8_t *order,
                        size_t order_size,
                        uint8_t page_mask_value,
                        int current_order_index)
{
    if (order_buffer_is_valid(order, order_size) &&
        is_order_index(current_order_index) &&
        page_is_enabled(order[current_order_index], page_mask_value)) {
        return current_order_index;
    }
    int first_index = first_enabled_index(order, order_size, page_mask_value);
    return index_found(first_index) ? first_index : 0;
}

bool normalize(uint8_t *order,
               size_t order_size,
               uint8_t page_mask_value,
               const uint8_t *default_order,
               size_t default_order_size)
{
    if (!order_buffer_is_valid(order, order_size) ||
        !order_is_valid(default_order, default_order_size)) {
        return false;
    }
    if (!order_is_valid(order, order_size)) {
        memcpy(order, default_order, order_size);
    }
    int first_enabled = first_enabled_index(order, order_size, page_mask_value);
    if (!index_found(first_enabled) ||
        order[first_enabled] != kWorkPageXiaozhiAI) {
        return true;
    }
    for (int i = first_enabled + 1; i < kWorkPageCount; ++i) {
        if (page_is_enabled(order[i], page_mask_value) &&
            order[i] != kWorkPageXiaozhiAI) {
            uint8_t replacement = order[i];
            order[i] = order[first_enabled];
            order[first_enabled] = replacement;
            break;
        }
    }
    return true;
}

} // namespace work_page_order_policy
