// 声明工作页顺序校验、启用页扫描和主页约束的纯数组策略。
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace work_page_order_policy {

inline constexpr int kInvalidIndex = -1;

bool is_work_page(int page);
bool is_order_index(int index);
bool index_found(int index);
bool order_is_valid(const uint8_t *order, size_t order_size);
bool page_is_enabled(int page, uint8_t page_mask);
bool mask_has_valid_home(uint8_t page_mask);
bool order_has_valid_home(const uint8_t *order,
                          size_t order_size,
                          uint8_t page_mask);
bool swap_entries_preserving_home(uint8_t *order,
                                  size_t order_size,
                                  uint8_t page_mask,
                                  int first_index,
                                  int second_index);
int index_of(const uint8_t *order, size_t order_size, int page);
int first_enabled_index(const uint8_t *order,
                        size_t order_size,
                        uint8_t page_mask);
int next_enabled_index(const uint8_t *order,
                       size_t order_size,
                       uint8_t page_mask,
                       int current_order_index);
int valid_enabled_index(const uint8_t *order,
                        size_t order_size,
                        uint8_t page_mask,
                        int current_order_index);
bool normalize(uint8_t *order,
               size_t order_size,
               uint8_t page_mask,
               const uint8_t *default_order,
               size_t default_order_size);

} // namespace work_page_order_policy
