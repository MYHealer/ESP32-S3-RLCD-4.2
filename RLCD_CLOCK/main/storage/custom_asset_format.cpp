// 实现自定义资源包的纯格式校验，供固件读取层与主机测试复用。
#include "custom_asset_format.h"

namespace {
constexpr uint16_t kBitsPerByte = 8;

static_assert(kBitsPerByte == 8, "custom assets expect 8-bit bytes");
static_assert(kCustomAssetCrc32Initial == 0xFFFFFFFFU,
              "custom asset CRC32 initial value must stay stable");
static_assert(kCustomAssetCrc32Polynomial == 0xEDB88320U,
              "custom asset CRC32 polynomial must stay stable");
}

bool custom_asset_header_identity_valid(const CustomAssetsHeader &header)
{
    return header.magic == kCustomAssetsMagic &&
           header.version == kCustomAssetsVersion &&
           header.entry_count > 0 &&
           header.entry_count <= kCustomAssetMaxEntries;
}

size_t custom_asset_entry_table_bytes(uint16_t entry_count)
{
    return static_cast<size_t>(entry_count) * sizeof(CustomAssetEntry);
}

size_t custom_asset_required_header_bytes(uint16_t entry_count)
{
    return sizeof(CustomAssetsHeader) + custom_asset_entry_table_bytes(entry_count);
}

CustomAssetHeaderLayoutStatus custom_asset_header_layout_status(
    const CustomAssetsHeader &header,
    size_t partition_size)
{
    if (header.header_size != custom_asset_required_header_bytes(header.entry_count)) {
        return CustomAssetHeaderLayoutStatus::kHeaderSizeMismatch;
    }
    if (header.total_size <= header.header_size) {
        return CustomAssetHeaderLayoutStatus::kEmptyPayload;
    }
    if (header.total_size > partition_size) {
        return CustomAssetHeaderLayoutStatus::kPackageTooLarge;
    }
    return CustomAssetHeaderLayoutStatus::kOk;
}

CustomAssetEntryBoundsStatus custom_asset_entry_bounds_status(
    const CustomAssetsHeader &header,
    const CustomAssetEntry &entry,
    size_t partition_size)
{
    if (entry.offset < header.header_size) {
        return CustomAssetEntryBoundsStatus::kBeforePayload;
    }
    if (entry.length == 0 || entry.offset > header.total_size) {
        return CustomAssetEntryBoundsStatus::kInvalidOffset;
    }
    if (entry.length > header.total_size - entry.offset) {
        return CustomAssetEntryBoundsStatus::kInvalidLength;
    }
    if (header.total_size > partition_size) {
        return CustomAssetEntryBoundsStatus::kPackageTooLarge;
    }
    return CustomAssetEntryBoundsStatus::kOk;
}

uint16_t custom_asset_packed_1bit_bytes_per_row(uint16_t width)
{
    return static_cast<uint16_t>((static_cast<size_t>(width) + kBitsPerByte - 1U) /
                                 kBitsPerByte);
}

bool custom_asset_text_metadata_valid(const CustomAssetEntry &entry)
{
    return entry.index == 0 &&
           entry.width == 0 &&
           entry.height == 0 &&
           entry.frame_count == 0 &&
           entry.bytes_per_row == 0;
}

bool custom_asset_text_length_valid(const CustomAssetEntry &entry, size_t max_len)
{
    return entry.length > 0 && entry.length <= max_len;
}

bool custom_asset_text_entry_valid(const CustomAssetEntry &entry, size_t max_len)
{
    return custom_asset_text_metadata_valid(entry) &&
           custom_asset_text_length_valid(entry, max_len);
}

uint32_t custom_asset_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    if (!data && len > 0) {
        return crc;
    }
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < kBitsPerByte; ++bit) {
            uint32_t mask = -(crc & 1U);
            crc = (crc >> 1) ^ (kCustomAssetCrc32Polynomial & mask);
        }
    }
    return crc;
}

uint32_t custom_asset_crc32_finalize(uint32_t crc)
{
    return ~crc;
}
