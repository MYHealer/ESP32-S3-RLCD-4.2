// 声明 OTA 版本比较、SHA256 文本校验和十六进制转换接口。
#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

inline constexpr size_t kOtaSha256ByteCount = 32;
inline constexpr size_t kOtaSha256HexLen = kOtaSha256ByteCount * 2;

namespace ota_validation_detail {
inline constexpr int kSemverComponentCount = 3;
inline constexpr char kHexDigits[] = "0123456789abcdef";
inline constexpr int64_t kUsPerSecond = 1000000;
inline constexpr int kBytesPerKiB = 1024;
inline constexpr int kHttpStatusSuccessFirst = 200;
inline constexpr int kHttpStatusSuccessAfterLast = 300;
inline constexpr int kHttpStatusMovedPermanently = 301;
inline constexpr int kHttpStatusFound = 302;
inline constexpr int kHttpStatusSeeOther = 303;
inline constexpr int kHttpStatusTemporaryRedirect = 307;
inline constexpr int kHttpStatusPermanentRedirect = 308;

inline int parse_semver_component(const char **cursor)
{
    if (!cursor || !*cursor) {
        return 0;
    }
    int value = 0;
    while (**cursor >= '0' && **cursor <= '9') {
        int digit = **cursor - '0';
        if (value <= (INT_MAX - digit) / 10) {
            value = value * 10 + digit;
        } else {
            value = INT_MAX;
        }
        ++(*cursor);
    }
    if (**cursor == '.') {
        ++(*cursor);
    }
    return value;
}

static_assert(kSemverComponentCount == 3,
              "OTA semantic version comparison expects three components");
static_assert(INT_MAX >= 9999, "OTA semantic version component range is unexpectedly small");
static_assert(kOtaSha256ByteCount == 32, "OTA SHA256 byte count must remain 32");
static_assert(kOtaSha256HexLen == kOtaSha256ByteCount * 2,
              "OTA SHA256 hex text must use two characters per byte");
static_assert(sizeof(kHexDigits) == 17, "OTA lowercase hex table must contain 16 digits and NUL");
static_assert(kUsPerSecond > 0, "OTA speed conversion requires a positive second");
static_assert(kBytesPerKiB == 1024, "OTA speed conversion must remain binary KiB");
static_assert(kHttpStatusSuccessFirst < kHttpStatusSuccessAfterLast,
              "OTA HTTP success status range must be ordered");
static_assert(kHttpStatusSuccessAfterLast < kHttpStatusMovedPermanently,
              "OTA HTTP success and redirect status ranges must not overlap");
static_assert(kHttpStatusMovedPermanently < kHttpStatusFound &&
                  kHttpStatusFound < kHttpStatusSeeOther &&
                  kHttpStatusSeeOther < kHttpStatusTemporaryRedirect &&
                  kHttpStatusTemporaryRedirect < kHttpStatusPermanentRedirect,
              "OTA HTTP redirect status constants must stay ordered");
} // namespace ota_validation_detail

inline int ota_compare_versions(const char *remote, const char *current)
{
    if (!remote || !current) {
        return 0;
    }
    if (*remote == 'v' || *remote == 'V') {
        ++remote;
    }
    if (*current == 'v' || *current == 'V') {
        ++current;
    }
    for (int i = 0; i < ota_validation_detail::kSemverComponentCount; ++i) {
        int remote_component = ota_validation_detail::parse_semver_component(&remote);
        int current_component = ota_validation_detail::parse_semver_component(&current);
        if (remote_component != current_component) {
            return remote_component > current_component ? 1 : -1;
        }
    }
    return strcmp(remote, current);
}

inline bool ota_valid_sha256_string(const char *text)
{
    if (!text || strlen(text) != kOtaSha256HexLen) {
        return false;
    }
    for (const char *cursor = text; *cursor; ++cursor) {
        if (!((*cursor >= '0' && *cursor <= '9') ||
              (*cursor >= 'a' && *cursor <= 'f') ||
              (*cursor >= 'A' && *cursor <= 'F'))) {
            return false;
        }
    }
    return true;
}

inline int ota_speed_kbps_for_window(int bytes, int64_t elapsed_us)
{
    if (bytes <= 0 || elapsed_us <= 0) {
        return 0;
    }
    return static_cast<int>(static_cast<int64_t>(bytes) * ota_validation_detail::kUsPerSecond /
                            elapsed_us / ota_validation_detail::kBytesPerKiB);
}

inline bool ota_is_http_redirect_status(int status)
{
    return status == ota_validation_detail::kHttpStatusMovedPermanently ||
           status == ota_validation_detail::kHttpStatusFound ||
           status == ota_validation_detail::kHttpStatusSeeOther ||
           status == ota_validation_detail::kHttpStatusTemporaryRedirect ||
           status == ota_validation_detail::kHttpStatusPermanentRedirect;
}

constexpr bool ota_is_http_success_status(int status)
{
    return status >= ota_validation_detail::kHttpStatusSuccessFirst &&
           status < ota_validation_detail::kHttpStatusSuccessAfterLast;
}

inline bool ota_backup_manifest_metadata_matches(const char *current_version,
                                                 const char *current_sha256,
                                                 int current_size,
                                                 const char *candidate_version,
                                                 const char *candidate_sha256,
                                                 int candidate_size)
{
    if (!current_version || !current_sha256 || !candidate_version || !candidate_sha256) {
        return false;
    }
    const bool versions_match = strcmp(candidate_version, current_version) == 0;
    const bool checksums_match = strcasecmp(candidate_sha256, current_sha256) == 0;
    const bool sizes_match = current_size <= 0 || candidate_size <= 0 || current_size == candidate_size;
    return versions_match && checksums_match && sizes_match;
}

inline bool ota_fixed_text_matches(const char *actual,
                                   size_t actual_capacity,
                                   const char *expected)
{
    if (!actual || actual_capacity == 0 || !expected || expected[0] == '\0') {
        return false;
    }
    const size_t expected_length = strlen(expected);
    if (expected_length >= actual_capacity) {
        return false;
    }
    return memcmp(actual, expected, expected_length) == 0 &&
           actual[expected_length] == '\0';
}

inline bool ota_app_description_matches_expected(
    const char *image_version,
    size_t image_version_capacity,
    const char *image_project_name,
    size_t image_project_name_capacity,
    const char *expected_version,
    const char *expected_project_name)
{
    return ota_fixed_text_matches(image_version,
                                  image_version_capacity,
                                  expected_version) &&
           ota_fixed_text_matches(image_project_name,
                                  image_project_name_capacity,
                                  expected_project_name);
}

inline void ota_sha256_to_hex(const uint8_t *hash, char *out, size_t out_len)
{
    if (!out) {
        return;
    }
    if (out_len <= kOtaSha256HexLen) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return;
    }
    if (!hash) {
        out[0] = '\0';
        return;
    }
    for (size_t i = 0; i < kOtaSha256ByteCount; ++i) {
        out[i * 2] = ota_validation_detail::kHexDigits[hash[i] >> 4];
        out[i * 2 + 1] = ota_validation_detail::kHexDigits[hash[i] & 0x0F];
    }
    out[kOtaSha256HexLen] = '\0';
}
