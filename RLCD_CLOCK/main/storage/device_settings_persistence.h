// 声明声音、工作页、小智自动返回和图库周期设置的持久化入口。
#pragma once

#include <stddef.h>
#include <stdint.h>

struct ChimeRuntimeSnapshot;

bool save_hourly_chime_setting();
bool set_chime_setting(const ChimeRuntimeSnapshot &settings);
bool set_work_page_enabled_mask_setting(uint8_t page_mask);
bool set_work_page_order_setting(const uint8_t *page_order,
                                 size_t page_order_size);
bool set_xiaozhi_auto_return_setting(bool enabled);
bool set_gallery_rotation_period_setting(uint8_t period);
