// 提供不依赖 Flash 分区的自定义资源包格式与边界校验。
#pragma once

#include "custom_assets.h"

#include <stddef.h>
#include <stdint.h>

enum class CustomAssetHeaderLayoutStatus {
    kOk = 0,
    kHeaderSizeMismatch,
    kEmptyPayload,
    kPackageTooLarge,
};

enum class CustomAssetEntryBoundsStatus {
    kOk = 0,
    kBeforePayload,
    kInvalidOffset,
    kInvalidLength,
    kPackageTooLarge,
};

inline constexpr uint32_t kCustomAssetCrc32Initial = 0xFFFFFFFFU;
inline constexpr uint32_t kCustomAssetCrc32Polynomial = 0xEDB88320U;

bool custom_asset_header_identity_valid(const CustomAssetsHeader &header);
size_t custom_asset_entry_table_bytes(uint16_t entry_count);
size_t custom_asset_required_header_bytes(uint16_t entry_count);
CustomAssetHeaderLayoutStatus custom_asset_header_layout_status(
    const CustomAssetsHeader &header,
    size_t partition_size);
CustomAssetEntryBoundsStatus custom_asset_entry_bounds_status(
    const CustomAssetsHeader &header,
    const CustomAssetEntry &entry,
    size_t partition_size);
uint16_t custom_asset_packed_1bit_bytes_per_row(uint16_t width);
bool custom_asset_text_metadata_valid(const CustomAssetEntry &entry);
bool custom_asset_text_length_valid(const CustomAssetEntry &entry, size_t max_len);
bool custom_asset_text_entry_valid(const CustomAssetEntry &entry, size_t max_len);
uint32_t custom_asset_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t custom_asset_crc32_finalize(uint32_t crc);
