// 声明工作页启用掩码和完整顺序的内部发布接口，仅供配置加载和持久化事务使用。
#pragma once

#include "ui_work_page_catalog.h"

bool work_page_catalog_init();
void work_page_enabled_mask_store(uint8_t page_mask);
void work_page_order_replace(const uint8_t *order, size_t order_size);
