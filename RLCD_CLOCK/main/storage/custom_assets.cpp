// 读取并校验上位机写入 assets 分区的自定义图片资源包。
#include "custom_assets_internal.h"

#include "app_constexpr.h"
#include "ascii_text.h"
#include "custom_asset_catalog.h"
#include "custom_asset_format.h"

#include "app_metadata.h"
#include "clock_gallery_contract.h"
#include "ota_manifest_limits.h"
#include "status_gif_contract.h"
#include "weather_city_contract.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"

#include <cstring>

#define CUSTOM_ASSETS_PARTITION_RANGE_INVALID_LOG_FORMAT "custom assets partition range invalid offset=0x%08lx length=%u partition=%lu"
#define CUSTOM_ASSETS_PARTITION_READ_FAILED_LOG_FORMAT "read assets partition failed: %s"
#define CUSTOM_ASSETS_READ_FAILED_LOG_FORMAT "read custom asset failed: %s"
#define CUSTOM_ASSETS_ENTRY_BEFORE_PAYLOAD_LOG_FORMAT "custom asset entry before payload type=%u index=%u"
#define CUSTOM_ASSETS_ENTRY_OFFSET_INVALID_LOG_FORMAT "custom asset entry offset invalid type=%u index=%u"
#define CUSTOM_ASSETS_ENTRY_LENGTH_INVALID_LOG_FORMAT "custom asset entry length invalid type=%u index=%u"
#define CUSTOM_ASSETS_ENTRY_SHAPE_INVALID_LOG_FORMAT "custom asset entry shape invalid type=%u index=%u size=%ux%u frames=%u row=%u length=%lu"
#define CUSTOM_ASSETS_HEADER_CRC_MISMATCH_LOG_FORMAT "custom assets header crc mismatch"
#define CUSTOM_ASSETS_PAYLOAD_RANGE_INVALID_LOG_FORMAT "custom assets payload range invalid header=%u total=%lu"
#define CUSTOM_ASSETS_PAYLOAD_CRC_MISMATCH_LOG_FORMAT "custom assets payload crc mismatch"
#define CUSTOM_ASSETS_DIAG_GIF_FRAME_READ_FAILED_LOG_FORMAT "custom assets diag: gif frame %d read failed"
#define CUSTOM_ASSETS_DIAG_GIF_FRAME_DENSITY_LOG_FORMAT "custom assets diag: gif frame %d black_bits=%d/%d"
#define CUSTOM_ASSETS_DIAG_INIT_ENTER_LOG_FORMAT "custom assets diag: init enter"
#define CUSTOM_ASSETS_DIAG_PARTITION_NOT_FOUND_LOG_FORMAT "custom assets diag: partition not found"
#define CUSTOM_ASSETS_DIAG_PARTITION_FOUND_LOG_FORMAT "custom assets diag: partition found address=0x%08lx size=%lu"
#define CUSTOM_ASSETS_DIAG_HEADER_READ_FAILED_LOG_FORMAT "custom assets diag: header read failed"
#define CUSTOM_ASSETS_DIAG_HEADER_LOG_FORMAT "custom assets diag: header magic=0x%08lx version=%u header=%u entries=%u total=%lu header_crc=0x%08lx payload_crc=0x%08lx"
#define CUSTOM_ASSETS_DIAG_NO_VALID_PACKAGE_LOG_FORMAT "custom assets diag: no valid package"
#define CUSTOM_ASSETS_HEADER_SIZE_INVALID_LOG_FORMAT "custom assets header size invalid"
#define CUSTOM_ASSETS_DIAG_ENTRIES_READ_FAILED_LOG_FORMAT "custom assets diag: entries read failed"
#define CUSTOM_ASSETS_DIAG_ENTRY_LOG_FORMAT "custom assets diag: entry[%d] type=%u index=%u size=%ux%u frames=%u row=%u offset=0x%08lx length=%lu crc=0x%08lx"
#define CUSTOM_ASSETS_DIAG_ENTRY_REJECTED_LOG_FORMAT "custom assets diag: entry[%d] rejected"
#define CUSTOM_ASSETS_DIAG_DUPLICATE_MAIN_GIF_LOG_FORMAT "custom assets diag: duplicate main gif entry"
#define CUSTOM_ASSETS_DIAG_DUPLICATE_GALLERY_LOG_FORMAT "custom assets diag: duplicate gallery entry index=%u"
#define CUSTOM_ASSETS_DIAG_DUPLICATE_CONFIG_LOG_FORMAT "custom assets diag: duplicate config entry type=%u"
#define CUSTOM_ASSETS_DIAG_READY_LOG_FORMAT "custom assets diag: ready main_gif=%d gallery=%d weather_city=%d ota_url=%d"
static const esp_partition_t *s_assets_partition = nullptr;
static CustomAssetsHeader s_assets_header = {};
static EXT_RAM_BSS_ATTR CustomAssetEntry s_entries[kCustomAssetMaxEntries] = {};
static int s_entry_count = 0;
static EXT_RAM_BSS_ATTR CustomAssetCatalog s_asset_catalog;
static constexpr const char *kCustomAssetsPartitionLabel = "assets";
static constexpr size_t kCustomAssetCrcIoChunkSize = 4 * 1024;
static constexpr size_t kCustomAssetDiagnosticIoChunkSize = 256;
// custom_assets_init() is the only caller of both scan paths, and it runs
// synchronously before application tasks are created. Reuse one external-RAM
// buffer so resource validation does not consume the small ESP main-task stack.
static EXT_RAM_BSS_ATTR uint8_t s_custom_asset_io_workspace[kCustomAssetCrcIoChunkSize] = {};
static constexpr int kCustomAssetDiagGifFrames[] = {0, 1, 30, 59};

constexpr bool custom_asset_diag_frames_valid()
{
    int previous = -1;
    for (int frame : kCustomAssetDiagGifFrames) {
        if (frame < 0 || frame >= STATUS_GIF_FRAME_COUNT || frame <= previous) {
            return false;
        }
        previous = frame;
    }
    return true;
}

constexpr bool custom_asset_type_ids_distinct()
{
    constexpr uint16_t types[] = {
        kCustomAssetTypeMainGif,
        kCustomAssetTypeGalleryImage,
        kCustomAssetTypeWeatherCity,
        kCustomAssetTypeOtaManifestUrl,
    };
    for (size_t i = 0; i < array_count(types); ++i) {
        for (size_t j = i + 1; j < array_count(types); ++j) {
            if (types[i] == types[j]) {
                return false;
            }
        }
    }
    return true;
}

static bool custom_asset_text_read_args_valid(const CustomAssetEntry *entry, const char *out, size_t out_len)
{
    return entry && out && out_len > 0 && entry->length < out_len;
}

static_assert(array_count(kCustomAssetDiagGifFrames) > 0,
              "custom asset diagnostic GIF frame table must not be empty");
static_assert(custom_asset_diag_frames_valid(),
              "custom asset diagnostic GIF frames must be ordered and valid");
static_assert(sizeof(CustomAssetsHeader) == 24, "custom assets header wire size must stay stable");
static_assert(sizeof(CustomAssetEntry) == 24, "custom asset entry wire size must stay stable");
static_assert(kCustomAssetsMagic == 0x31414357, "custom assets magic must remain WCA1");
static_assert(kCustomAssetsVersion > 0, "custom assets version must be positive");
static_assert(custom_asset_type_ids_distinct(), "custom asset type ids must be distinct");
static_assert(kCustomAssetWeatherCityMaxLen + 1 <= kManualWeatherCityLen,
              "custom weather city must fit manual city storage");
static_assert(kCustomAssetOtaManifestUrlMaxLen + 1 <= kOtaUrlLen,
              "custom OTA manifest URL must fit OTA URL storage");
static_assert(kCustomAssetMaxEntries > 0, "custom asset entry limit must be positive");
static_assert(CustomAssetCatalog::kMaxGalleryImages > 0, "custom gallery image limit must be positive");
static_assert(CustomAssetCatalog::kMaxGalleryImages <= kCustomAssetMaxEntries,
              "custom gallery image limit must fit entry table");
static_assert(1 + CustomAssetCatalog::kMaxGalleryImages + 2 <= kCustomAssetMaxEntries,
              "custom entry table must fit GIF, gallery and configuration entries");
static_assert(sizeof(CustomAssetsHeader) +
                      kCustomAssetMaxEntries * sizeof(CustomAssetEntry) <=
                  UINT16_MAX,
              "custom asset header must fit its 16-bit wire size");
static_assert(kCustomAssetCrcIoChunkSize > 0,
              "custom asset CRC I/O chunk size must be positive");
static_assert(kCustomAssetCrcIoChunkSize <= 4 * 1024,
              "custom asset CRC I/O chunk must stay within the validated workspace size");
static_assert(kCustomAssetDiagnosticIoChunkSize > 0,
              "custom asset diagnostic I/O chunk size must be positive");
static_assert(kCustomAssetDiagnosticIoChunkSize <= kCustomAssetCrcIoChunkSize,
              "custom asset diagnostic I/O chunk must fit the shared workspace");
static bool partition_range_valid(uint32_t offset, size_t length)
{
    if (!s_assets_partition) {
        return false;
    }
    if (offset > s_assets_partition->size || length > s_assets_partition->size - offset) {
        ESP_LOGW(TAG,
                 CUSTOM_ASSETS_PARTITION_RANGE_INVALID_LOG_FORMAT,
                 (unsigned long)offset,
                 (unsigned)length,
                 (unsigned long)s_assets_partition->size);
        return false;
    }
    return true;
}

static bool partition_io_args_valid(const void *out)
{
    return s_assets_partition && out;
}

static bool partition_crc(uint32_t offset, uint32_t length, uint32_t *crc_out)
{
    if (!partition_io_args_valid(crc_out)) {
        return false;
    }
    if (!partition_range_valid(offset, length)) {
        return false;
    }
    uint32_t crc = kCustomAssetCrc32Initial;
    uint32_t remaining = length;
    uint32_t cursor = offset;
    while (remaining > 0) {
        size_t chunk = remaining > kCustomAssetCrcIoChunkSize
                           ? kCustomAssetCrcIoChunkSize
                           : remaining;
        esp_err_t err = esp_partition_read(s_assets_partition,
                                           cursor,
                                           s_custom_asset_io_workspace,
                                           chunk);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, CUSTOM_ASSETS_PARTITION_READ_FAILED_LOG_FORMAT, esp_err_to_name(err));
            return false;
        }
        crc = custom_asset_crc32_update(crc,
                                        s_custom_asset_io_workspace,
                                        chunk);
        cursor += chunk;
        remaining -= chunk;
    }
    *crc_out = custom_asset_crc32_finalize(crc);
    return true;
}

static bool read_checked(uint32_t offset, void *out, size_t len)
{
    if (!partition_io_args_valid(out)) {
        return false;
    }
    if (!partition_range_valid(offset, len)) {
        return false;
    }
    esp_err_t err = esp_partition_read(s_assets_partition, offset, out, len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_READ_FAILED_LOG_FORMAT, esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool validate_entry_bounds(const CustomAssetEntry &entry)
{
    if (!s_assets_partition) {
        return false;
    }
    switch (custom_asset_entry_bounds_status(s_assets_header,
                                             entry,
                                             s_assets_partition->size)) {
    case CustomAssetEntryBoundsStatus::kOk:
        return true;
    case CustomAssetEntryBoundsStatus::kBeforePayload:
        ESP_LOGW(TAG, CUSTOM_ASSETS_ENTRY_BEFORE_PAYLOAD_LOG_FORMAT, entry.type, entry.index);
        return false;
    case CustomAssetEntryBoundsStatus::kInvalidOffset:
        ESP_LOGW(TAG, CUSTOM_ASSETS_ENTRY_OFFSET_INVALID_LOG_FORMAT, entry.type, entry.index);
        return false;
    case CustomAssetEntryBoundsStatus::kInvalidLength:
        ESP_LOGW(TAG, CUSTOM_ASSETS_ENTRY_LENGTH_INVALID_LOG_FORMAT, entry.type, entry.index);
        return false;
    case CustomAssetEntryBoundsStatus::kPackageTooLarge:
        return false;
    }
    return false;
}

static size_t custom_asset_entry_table_bytes()
{
    return ::custom_asset_entry_table_bytes(static_cast<uint16_t>(s_entry_count));
}

static size_t custom_asset_gallery_image_bytes()
{
    return (size_t)CLOCK_GALLERY_IMAGE_BYTES_PER_ROW * CLOCK_GALLERY_IMAGE_HEIGHT;
}

static bool validate_entry_shape(const CustomAssetEntry &entry)
{
    bool valid = false;
    if (entry.type == kCustomAssetTypeMainGif) {
        size_t expected = (size_t)STATUS_GIF_FRAME_COUNT * STATUS_GIF_BYTES_PER_FRAME;
        bool row_ok = entry.bytes_per_row == 0 ||
                      entry.bytes_per_row == custom_asset_packed_1bit_bytes_per_row(STATUS_GIF_WIDTH);
        valid = entry.index == 0 &&
                entry.width == STATUS_GIF_WIDTH &&
                entry.height == STATUS_GIF_HEIGHT &&
                entry.frame_count == STATUS_GIF_FRAME_COUNT &&
                row_ok &&
                entry.length == expected;
    } else if (entry.type == kCustomAssetTypeGalleryImage) {
        size_t expected = custom_asset_gallery_image_bytes();
        valid = entry.index < CustomAssetCatalog::kMaxGalleryImages &&
                entry.width == CLOCK_GALLERY_IMAGE_WIDTH &&
                entry.height == CLOCK_GALLERY_IMAGE_HEIGHT &&
                entry.frame_count == 1 &&
                entry.bytes_per_row == CLOCK_GALLERY_IMAGE_BYTES_PER_ROW &&
                entry.length == expected;
    } else if (entry.type == kCustomAssetTypeWeatherCity) {
        valid = custom_asset_text_entry_valid(entry, kCustomAssetWeatherCityMaxLen);
    } else if (entry.type == kCustomAssetTypeOtaManifestUrl) {
        valid = custom_asset_text_entry_valid(entry, kCustomAssetOtaManifestUrlMaxLen);
    }
    if (!valid) {
        ESP_LOGW(TAG,
                 CUSTOM_ASSETS_ENTRY_SHAPE_INVALID_LOG_FORMAT,
                 (unsigned)entry.type,
                 (unsigned)entry.index,
                 (unsigned)entry.width,
                 (unsigned)entry.height,
                 (unsigned)entry.frame_count,
                 (unsigned)entry.bytes_per_row,
                 (unsigned long)entry.length);
    }
    return valid;
}

static bool validate_header_crc()
{
    CustomAssetsHeader header = s_assets_header;
    header.header_crc = 0;
    uint32_t crc = kCustomAssetCrc32Initial;
    crc = custom_asset_crc32_update(crc,
                                    reinterpret_cast<const uint8_t *>(&header),
                                    sizeof(header));
    crc = custom_asset_crc32_update(crc,
                                    reinterpret_cast<const uint8_t *>(s_entries),
                                    custom_asset_entry_table_bytes());
    crc = custom_asset_crc32_finalize(crc);
    if (crc != s_assets_header.header_crc) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_HEADER_CRC_MISMATCH_LOG_FORMAT);
        return false;
    }
    return true;
}

static bool validate_payload_crc()
{
    if (s_assets_header.header_size > s_assets_header.total_size) {
        ESP_LOGW(TAG,
                 CUSTOM_ASSETS_PAYLOAD_RANGE_INVALID_LOG_FORMAT,
                 (unsigned)s_assets_header.header_size,
                 (unsigned long)s_assets_header.total_size);
        return false;
    }
    uint32_t payload_offset = s_assets_header.header_size;
    uint32_t payload_length = s_assets_header.total_size - payload_offset;
    uint32_t crc = 0;
    if (!partition_crc(payload_offset, payload_length, &crc)) {
        return false;
    }
    if (crc != s_assets_header.payload_crc) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_PAYLOAD_CRC_MISMATCH_LOG_FORMAT);
        return false;
    }
    return true;
}

static void reset_custom_assets()
{
    s_assets_header = {};
    memset(s_entries, 0, sizeof(s_entries));
    s_entry_count = 0;
    s_asset_catalog.reset();
}

static int count_black_bits(const uint8_t *data, size_t len)
{
    if (!data) {
        return 0;
    }
    int total = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t value = data[i];
        while (value) {
            total += value & 1U;
            value >>= 1;
        }
    }
    return total;
}

static bool partition_black_bit_count(uint32_t offset, uint32_t length, int *count_out)
{
    if (!count_out || !partition_range_valid(offset, length)) {
        return false;
    }
    uint32_t remaining = length;
    uint32_t cursor = offset;
    int total = 0;
    while (remaining > 0) {
        size_t chunk = remaining > kCustomAssetDiagnosticIoChunkSize
                           ? kCustomAssetDiagnosticIoChunkSize
                           : remaining;
        if (!read_checked(cursor, s_custom_asset_io_workspace, chunk)) {
            return false;
        }
        total += count_black_bits(s_custom_asset_io_workspace, chunk);
        cursor += chunk;
        remaining -= chunk;
    }
    *count_out = total;
    return true;
}

static void log_custom_gif_frame_density(int frame)
{
    const CustomAssetEntry *main_gif_entry = s_asset_catalog.main_gif();
    if (!main_gif_entry || frame < 0 || frame >= STATUS_GIF_FRAME_COUNT) {
        return;
    }
    uint32_t offset = main_gif_entry->offset + (uint32_t)frame * STATUS_GIF_BYTES_PER_FRAME;
    int black_bits = 0;
    if (!partition_black_bit_count(offset, STATUS_GIF_BYTES_PER_FRAME, &black_bits)) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_GIF_FRAME_READ_FAILED_LOG_FORMAT, frame);
        return;
    }
    ESP_LOGD(TAG,
             CUSTOM_ASSETS_DIAG_GIF_FRAME_DENSITY_LOG_FORMAT,
             frame,
             black_bits,
             STATUS_GIF_WIDTH * STATUS_GIF_HEIGHT);
}

static bool read_and_validate_custom_asset_entry_table()
{
    s_entry_count = s_assets_header.entry_count;
    if (custom_asset_header_layout_status(s_assets_header,
                                          s_assets_partition->size) !=
        CustomAssetHeaderLayoutStatus::kOk) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_HEADER_SIZE_INVALID_LOG_FORMAT);
        return false;
    }
    if (!read_checked(sizeof(CustomAssetsHeader), s_entries, custom_asset_entry_table_bytes())) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_ENTRIES_READ_FAILED_LOG_FORMAT);
        return false;
    }
    return validate_header_crc() && validate_payload_crc();
}

static bool register_custom_asset_entry(int entry_index)
{
    CustomAssetEntry &entry = s_entries[entry_index];
    ESP_LOGI(TAG,
             CUSTOM_ASSETS_DIAG_ENTRY_LOG_FORMAT,
             entry_index,
             (unsigned)entry.type,
             (unsigned)entry.index,
             (unsigned)entry.width,
             (unsigned)entry.height,
             (unsigned)entry.frame_count,
             (unsigned)entry.bytes_per_row,
             (unsigned long)entry.offset,
             (unsigned long)entry.length,
             (unsigned long)entry.crc32);
    // Header CRC protects entry metadata and payload CRC already covers every
    // accepted entry byte, so rescanning each entry would only duplicate Flash I/O.
    if (!validate_entry_bounds(entry) || !validate_entry_shape(entry)) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_ENTRY_REJECTED_LOG_FORMAT, entry_index);
        return false;
    }
    CustomAssetCatalogStatus status = s_asset_catalog.add(&entry);
    switch (status) {
    case CustomAssetCatalogStatus::kAdded:
    case CustomAssetCatalogStatus::kIgnoredGalleryCapacity:
    case CustomAssetCatalogStatus::kIgnoredUnsupportedType:
        return true;
    case CustomAssetCatalogStatus::kDuplicateMainGif:
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_DUPLICATE_MAIN_GIF_LOG_FORMAT);
        return false;
    case CustomAssetCatalogStatus::kDuplicateGallery:
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_DUPLICATE_GALLERY_LOG_FORMAT, entry.index);
        return false;
    case CustomAssetCatalogStatus::kDuplicateConfig:
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_DUPLICATE_CONFIG_LOG_FORMAT, entry.type);
        return false;
    }
    return false;
}

static void log_custom_assets_ready_state()
{
    ESP_LOGI(TAG,
             CUSTOM_ASSETS_DIAG_READY_LOG_FORMAT,
             s_asset_catalog.main_gif() ? 1 : 0,
             s_asset_catalog.gallery_count(),
             s_asset_catalog.weather_city() ? 1 : 0,
             s_asset_catalog.ota_manifest_url() ? 1 : 0);
    if (s_asset_catalog.main_gif() &&
        esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
        for (int frame : kCustomAssetDiagGifFrames) {
            log_custom_gif_frame_density(frame);
        }
    }
}

void custom_assets_init()
{
    ESP_LOGI(TAG, CUSTOM_ASSETS_DIAG_INIT_ENTER_LOG_FORMAT);
    reset_custom_assets();
    s_assets_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                  ESP_PARTITION_SUBTYPE_ANY,
                                                  kCustomAssetsPartitionLabel);
    if (!s_assets_partition) {
        ESP_LOGI(TAG, CUSTOM_ASSETS_DIAG_PARTITION_NOT_FOUND_LOG_FORMAT);
        return;
    }
    ESP_LOGI(TAG,
             CUSTOM_ASSETS_DIAG_PARTITION_FOUND_LOG_FORMAT,
             (unsigned long)s_assets_partition->address,
             (unsigned long)s_assets_partition->size);
    if (!read_checked(0, &s_assets_header, sizeof(s_assets_header))) {
        ESP_LOGW(TAG, CUSTOM_ASSETS_DIAG_HEADER_READ_FAILED_LOG_FORMAT);
        return;
    }
    ESP_LOGI(TAG,
             CUSTOM_ASSETS_DIAG_HEADER_LOG_FORMAT,
             (unsigned long)s_assets_header.magic,
             (unsigned)s_assets_header.version,
             (unsigned)s_assets_header.header_size,
             (unsigned)s_assets_header.entry_count,
             (unsigned long)s_assets_header.total_size,
             (unsigned long)s_assets_header.header_crc,
             (unsigned long)s_assets_header.payload_crc);
    if (!custom_asset_header_identity_valid(s_assets_header)) {
        ESP_LOGI(TAG, CUSTOM_ASSETS_DIAG_NO_VALID_PACKAGE_LOG_FORMAT);
        return;
    }
    if (!read_and_validate_custom_asset_entry_table()) {
        reset_custom_assets();
        return;
    }

    for (int i = 0; i < s_entry_count; ++i) {
        if (!register_custom_asset_entry(i)) {
            reset_custom_assets();
            return;
        }
    }
    s_asset_catalog.sort_gallery_by_index();
    log_custom_assets_ready_state();
}

bool custom_assets_available()
{
    return s_asset_catalog.available();
}

int custom_assets_gallery_count()
{
    return s_asset_catalog.gallery_count();
}

bool custom_assets_read_main_gif_frame(int frame, uint8_t *out, size_t out_len)
{
    const CustomAssetEntry *main_gif_entry = s_asset_catalog.main_gif();
    if (!main_gif_entry || !out || out_len < STATUS_GIF_BYTES_PER_FRAME) {
        return false;
    }
    if (frame < 0) {
        frame = 0;
    } else if (frame >= STATUS_GIF_FRAME_COUNT) {
        frame = STATUS_GIF_FRAME_COUNT - 1;
    }
    uint32_t offset = main_gif_entry->offset + (uint32_t)frame * STATUS_GIF_BYTES_PER_FRAME;
    return read_checked(offset, out, STATUS_GIF_BYTES_PER_FRAME);
}

bool custom_assets_read_gallery_image(int index, uint8_t *out, size_t out_len)
{
    int gallery_count = s_asset_catalog.gallery_count();
    if (gallery_count <= 0 || !out) {
        return false;
    }
    size_t expected = custom_asset_gallery_image_bytes();
    if (out_len < expected) {
        return false;
    }
    if (index < 0) {
        index = 0;
    }
    const CustomAssetEntry *entry = s_asset_catalog.gallery_at(index % gallery_count);
    if (!entry) {
        return false;
    }
    return read_checked(entry->offset, out, expected);
}

static bool custom_assets_read_text(const CustomAssetEntry *entry, char *out, size_t out_len)
{
    if (!custom_asset_text_read_args_valid(entry, out, out_len)) {
        return false;
    }
    memset(out, 0, out_len);
    if (!read_checked(entry->offset, out, entry->length)) {
        out[0] = '\0';
        return false;
    }
    out[entry->length] = '\0';
    trim_ascii_line_whitespace(out);
    return out[0] != '\0';
}

bool custom_assets_read_weather_city(char *out, size_t out_len)
{
    return custom_assets_read_text(s_asset_catalog.weather_city(), out, out_len);
}

bool custom_assets_read_ota_manifest_url(char *out, size_t out_len)
{
    return custom_assets_read_text(s_asset_catalog.ota_manifest_url(), out, out_len);
}
