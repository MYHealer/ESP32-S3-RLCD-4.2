// 声明工作页开关与顺序的 NVS 存储边界。
#pragma once

#include <esp_err.h>
#include <nvs.h>
#include <stddef.h>
#include <stdint.h>

namespace network_page_storage {
inline constexpr const char *kPageMaskV1Key = "page_mask_v1";
inline constexpr const char *kPageMaskV2Key = "page_mask_v2";
inline constexpr const char *kPageMaskV3Key = "page_mask_v3";
inline constexpr const char *kPageMaskV4Key = "page_mask_v4";
inline constexpr const char *kPageMaskV5Key = "page_mask_v5";
inline constexpr const char *kPageMaskV6Key = "page_mask_v6";
inline constexpr const char *kPageOrderV1Key = "page_order_v1";
inline constexpr const char *kPageOrderV2Key = "page_order_v2";
inline constexpr const char *kPageOrderV3Key = "page_order_v3";
inline constexpr const char *kPageOrderV4Key = "page_order_v4";
inline constexpr const char *kPageOrderV5Key = "page_order_v5";
inline constexpr const char *kPageOrderV6Key = "page_order_v6";

uint8_t read_saved_page_mask(nvs_handle_t nvs);
bool read_saved_page_order(nvs_handle_t nvs, uint8_t *page_order, size_t page_order_size);
esp_err_t write_work_page_order_nvs(nvs_handle_t nvs,
                                    esp_err_t err,
                                    const uint8_t *page_order,
                                    size_t page_order_size,
                                    bool *changed);
} // namespace network_page_storage
