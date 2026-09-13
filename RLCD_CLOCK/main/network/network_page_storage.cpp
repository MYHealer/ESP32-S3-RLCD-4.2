// 负责工作页开关、顺序和上一版页面数据的 NVS 读写。
#include "network_page_storage.h"

#include "network_page_storage_policy.h"
#include "ui_work_page_catalog.h"

#include <string.h>

namespace network_page_storage {
namespace {
bool saved_page_order_matches(nvs_handle_t nvs,
                              const uint8_t *page_order,
                              size_t page_order_size)
{
    uint8_t saved_order[kWorkPageCount] = {};
    size_t stored_len = sizeof(saved_order);
    if (page_order &&
        page_order_size == sizeof(saved_order) &&
        nvs_get_blob(nvs, kPageOrderV7Key, saved_order, &stored_len) == ESP_OK &&
        stored_len == sizeof(saved_order) &&
        memcmp(saved_order, page_order, page_order_size) == 0) {
        return true;
    }
    // Fallback: check V6 (8-page order, size mismatch means not equal)
    stored_len = kWorkPageCount - 1;
    uint8_t v6_order[8] = {};
    return page_order &&
           nvs_get_blob(nvs, kPageOrderV6Key, v6_order, &stored_len) == ESP_OK &&
           stored_len == 8 &&
           memcmp(v6_order, page_order, 8) == 0 &&
           page_order[8] == kWorkPageMiPlayPlayer;
}
} // namespace

WorkPageMask read_saved_page_mask(nvs_handle_t nvs)
{
    WorkPageMask page_mask = kCurrentKnownPageMask;
    // V7: u16 mask (supports 9+ pages)
    uint16_t mask16 = 0;
    if (nvs_get_u16(nvs, kPageMaskV7Key, &mask16) == ESP_OK) {
        return normalize_work_page_enabled_mask(static_cast<WorkPageMask>(mask16));
    }
    // V6: u8 mask (8 pages max), auto-enable MiPlayPlayer
    uint8_t mask8 = 0;
    if (nvs_get_u8(nvs, kPageMaskV6Key, &mask8) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            static_cast<WorkPageMask>(mask8) | (1U << kWorkPageMiPlayPlayer));
    }
    if (nvs_get_u8(nvs, kPageMaskV5Key, &mask8) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            static_cast<WorkPageMask>(mask8) | (1U << kWorkPageAggregateClock) |
            (1U << kWorkPageMiPlayPlayer));
    }
    if (nvs_get_u8(nvs, kPageMaskV4Key, &mask8) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            migrate_v4_page_mask(mask8) | (1U << kWorkPageAggregateClock) |
            (1U << kWorkPageMiPlayPlayer));
    }
    return page_mask;
}

bool read_saved_page_order(nvs_handle_t nvs, uint8_t *page_order, size_t page_order_size)
{
    if (!page_order || page_order_size != kWorkPageCount) {
        return false;
    }
    // V7: 9-page order
    size_t stored_len = page_order_size;
    if (nvs_get_blob(nvs, kPageOrderV7Key, page_order, &stored_len) == ESP_OK &&
        stored_len == page_order_size) {
        return true;
    }
    // V6: 8-page order, append MiPlayPlayer at end
    uint8_t v6_order[8] = {};
    stored_len = 8;
    if (nvs_get_blob(nvs, kPageOrderV6Key, v6_order, &stored_len) == ESP_OK &&
        stored_len == 8) {
        memcpy(page_order, v6_order, 8);
        page_order[8] = kWorkPageMiPlayPlayer;
        return true;
    }
    // V5: AggregateClock as last page
    stored_len = kWorkPageAggregateClock;
    if (nvs_get_blob(nvs, kPageOrderV5Key, page_order, &stored_len) == ESP_OK &&
        stored_len == kWorkPageAggregateClock) {
        page_order[kWorkPageAggregateClock] = kWorkPageAggregateClock;
        page_order[kWorkPageMiPlayPlayer] = kWorkPageMiPlayPlayer;
        return true;
    }
    uint8_t legacy_order[kLegacyV4WorkPageCount] = {};
    stored_len = sizeof(legacy_order);
    if (nvs_get_blob(nvs, kPageOrderV4Key, legacy_order, &stored_len) == ESP_OK &&
        stored_len == sizeof(legacy_order)) {
        return migrate_v4_page_order(legacy_order, sizeof(legacy_order),
                                     page_order, page_order_size);
    }
    return false;
}

esp_err_t write_work_page_order_nvs(nvs_handle_t nvs,
                                    esp_err_t err,
                                    const uint8_t *page_order,
                                    size_t page_order_size,
                                    bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (saved_page_order_matches(nvs, page_order, page_order_size)) {
        return ESP_OK;
    }
    esp_err_t write_err = nvs_set_blob(nvs, kPageOrderV7Key, page_order, page_order_size);
    if (write_err == ESP_OK && changed) {
        *changed = true;
    }
    return write_err;
}
} // namespace network_page_storage
