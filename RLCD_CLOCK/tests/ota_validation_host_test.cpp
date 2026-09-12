// 验证 OTA 版本、SHA、下载速度、重定向和备用清单一致性纯规则。
#include "ota_validation.h"

#include <assert.h>
#include <string.h>

int main()
{
    assert(ota_compare_versions("v1.5.6", "v1.5.5") > 0);
    assert(ota_compare_versions("1.5.5", "v1.5.5") == 0);
    assert(ota_compare_versions("v1.4.9", "v1.5.0") < 0);
    assert(ota_compare_versions(nullptr, "v1.5.5") == 0);
    assert(ota_compare_versions("v999999999999999999999.0.0", "v1.5.5") > 0);
    assert(ota_compare_versions("v999999999999999999999.1.0",
                                "v999999999999999999999.0.9") > 0);

    constexpr const char *kLowerSha =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    constexpr const char *kUpperSha =
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    assert(ota_valid_sha256_string(kLowerSha));
    assert(ota_valid_sha256_string(kUpperSha));
    assert(!ota_valid_sha256_string("0123"));
    assert(!ota_valid_sha256_string(
        "g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    assert(!ota_valid_sha256_string(nullptr));

    uint8_t hash[kOtaSha256ByteCount] = {};
    hash[0] = 0xab;
    hash[kOtaSha256ByteCount - 1] = 0xcd;
    char hash_text[kOtaSha256HexLen + 1] = {};
    ota_sha256_to_hex(hash, hash_text, sizeof(hash_text));
    assert(strlen(hash_text) == kOtaSha256HexLen);
    assert(hash_text[0] == 'a' && hash_text[1] == 'b');
    assert(hash_text[kOtaSha256HexLen - 2] == 'c');
    assert(hash_text[kOtaSha256HexLen - 1] == 'd');
    char short_hash[4] = {'x', 'x', 'x', '\0'};
    ota_sha256_to_hex(hash, short_hash, sizeof(short_hash));
    assert(short_hash[0] == '\0');

    assert(ota_speed_kbps_for_window(2048, 1000000) == 2);
    assert(ota_speed_kbps_for_window(3072, 1500000) == 2);
    assert(ota_speed_kbps_for_window(0, 1000000) == 0);
    assert(ota_speed_kbps_for_window(2048, 0) == 0);

    assert(ota_is_http_redirect_status(301));
    assert(ota_is_http_redirect_status(302));
    assert(ota_is_http_redirect_status(303));
    assert(ota_is_http_redirect_status(307));
    assert(ota_is_http_redirect_status(308));
    assert(!ota_is_http_redirect_status(200));
    assert(!ota_is_http_redirect_status(300));
    assert(!ota_is_http_redirect_status(304));

    assert(!ota_is_http_success_status(199));
    assert(ota_is_http_success_status(200));
    assert(ota_is_http_success_status(204));
    assert(ota_is_http_success_status(299));
    assert(!ota_is_http_success_status(300));
    assert(!ota_is_http_success_status(302));

    assert(ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 1234,
                                                "v1.5.5", kUpperSha, 1234));
    assert(ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 0,
                                                "v1.5.5", kUpperSha, 4321));
    assert(ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 1234,
                                                "v1.5.5", kUpperSha, 0));
    assert(!ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 1234,
                                                 "v1.5.6", kUpperSha, 1234));
    assert(!ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 1234,
                                                 "v1.5.5", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 1234));
    assert(!ota_backup_manifest_metadata_matches("v1.5.5", kLowerSha, 1234,
                                                 "v1.5.5", kUpperSha, 4321));
    assert(!ota_backup_manifest_metadata_matches(nullptr, kLowerSha, 1234,
                                                 "v1.5.5", kUpperSha, 1234));

    const char image_version[32] = "v1.5.28";
    const char old_image_version[32] = "v1.5.27";
    const char image_project[32] = "weather_clock";
    const char other_image_project[32] = "other_project";
    const char empty_image_version[32] = {};
    assert(ota_app_description_matches_expected(
        image_version, sizeof(image_version),
        image_project, sizeof(image_project),
        "v1.5.28", "weather_clock"));
    assert(!ota_app_description_matches_expected(
        old_image_version, sizeof(old_image_version),
        image_project, sizeof(image_project),
        "v1.5.28", "weather_clock"));
    assert(!ota_app_description_matches_expected(
        image_version, sizeof(image_version),
        other_image_project, sizeof(other_image_project),
        "v1.5.28", "weather_clock"));
    assert(!ota_app_description_matches_expected(
        empty_image_version, sizeof(empty_image_version),
        image_project, sizeof(image_project),
        "v1.5.28", "weather_clock"));
    assert(!ota_app_description_matches_expected(
        image_version, sizeof(image_version),
        image_project, sizeof(image_project),
        "", "weather_clock"));
    assert(!ota_app_description_matches_expected(
        nullptr, sizeof(image_version),
        image_project, sizeof(image_project),
        "v1.5.28", "weather_clock"));
    const char unterminated_version[] = {'v', '1', '.', '5'};
    assert(!ota_app_description_matches_expected(
        unterminated_version,
        sizeof(unterminated_version),
        image_project,
        sizeof(image_project),
        "v1.5",
        "weather_clock"));

    return 0;
}
