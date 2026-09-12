// 获取自定义、GitHub 与 Gitee OTA manifest，并维护安装前的运行态清单缓存。
#include "ota_manifest_client.h"
#include "ota_manifest_client_internal.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_network_config.h"
#include "app_text_format.h"
#include "custom_assets.h"
#include "network_http_client.h"
#include "ota_validation.h"
#include "scoped_heap_buffer.h"

#include <esp_attr.h>
#include "esp_err.h"
#include "esp_log.h"

#include <string.h>

namespace {
constexpr size_t kManifestResponseBufferSize = 2048;
constexpr const char *kManifestSourceGithub = "GitHub";
constexpr const char *kManifestSourceGitee = "Gitee";
constexpr const char *kManifestSourceCustom = "Custom";
constexpr int kBuiltInManifestSourceCount = 2;
constexpr OtaManifestSource kBuiltInManifestSources[] = {
    {kManifestSourceGithub, kOtaManifestUrl},
    {kManifestSourceGitee, kOtaBackupManifestUrl},
};
EXT_RAM_BSS_ATTR OtaManifest s_cached_manifest;
constexpr const char *kManifestParseInvalidArgLog = "OTA manifest parse invalid arg";
constexpr const char *kManifestJsonParseFailedLog = "OTA manifest JSON parse failed";
#define MANIFEST_MISSING_REQUIRED_FIELDS_FORMAT "OTA manifest missing required fields version=%d url=%d sha=%d"
#define MANIFEST_SHA_INVALID_FORMAT "OTA manifest sha invalid len=%u"
#define MANIFEST_SOURCE_SKIPPED_FORMAT "OTA manifest source skipped: %s"
constexpr const char *kManifestResponseAllocFailedLog = "OTA manifest response alloc failed";
#define MANIFEST_FETCH_FAILED_FORMAT "OTA manifest failed source=%s err=%s"
#define MANIFEST_PARSE_FAILED_FORMAT "OTA manifest parse failed source=%s"
#define MANIFEST_LOADED_FORMAT "OTA manifest loaded source=%s version=%s"
#define BACKUP_MANIFEST_MISMATCH_FORMAT "OTA backup manifest mismatch current=%s backup=%s"
constexpr bool manifest_source_name_fits(const char *text)
{
    return cstr_nonempty(text) && cstr_length(text) < kOtaManifestSourceNameLen;
}

static_assert(kManifestResponseBufferSize > 1,
              "OTA manifest response buffer must fit text and NUL");
static_assert(array_count(kBuiltInManifestSources) == kBuiltInManifestSourceCount,
              "OTA built-in manifest source list must cover GitHub and Gitee");
static_assert(manifest_source_name_fits(kManifestSourceGithub),
              "GitHub OTA manifest source name must fit UI storage");
static_assert(manifest_source_name_fits(kManifestSourceGitee),
              "Gitee OTA manifest source name must fit UI storage");
static_assert(manifest_source_name_fits(kManifestSourceCustom),
              "custom OTA manifest source name must fit UI storage");
static_assert(manifest_source_name_fits(kOtaUnknownManifestSource),
              "unknown OTA manifest source name must fit UI storage");

void notify_manifest_failure(OtaManifestFailureCallback callback)
{
    if (callback) {
        callback();
    }
}

void clear_manifest_output(OtaManifest *manifest)
{
    if (manifest) {
        *manifest = OtaManifest{};
    }
}

void clear_text_output(char *out, size_t out_len)
{
    if (app_text::output_buffer_available(out, out_len)) {
        out[0] = '\0';
    }
}

bool parse_manifest_with_log(const char *json, OtaManifest *manifest)
{
    OtaManifestParseResult result = ota_parse_manifest_json(json, manifest);
    switch (result.status) {
    case kOtaManifestParseOk:
        return true;
    case kOtaManifestParseInvalidArgument:
        ESP_LOGW(TAG, "%s", kManifestParseInvalidArgLog);
        break;
    case kOtaManifestParseInvalidJson:
        ESP_LOGW(TAG, "%s", kManifestJsonParseFailedLog);
        break;
    case kOtaManifestParseMissingRequiredFields:
        ESP_LOGW(TAG,
                 MANIFEST_MISSING_REQUIRED_FIELDS_FORMAT,
                 result.have_version,
                 result.have_url,
                 result.have_sha256);
        break;
    case kOtaManifestParseInvalidSha256:
        ESP_LOGW(TAG, MANIFEST_SHA_INVALID_FORMAT, (unsigned)result.sha256_length);
        break;
    }
    return false;
}

bool fetch_manifest_from_source(const OtaManifestSource &source,
                                OtaManifest *manifest,
                                ScopedHeapBuffer<char> &response,
                                OtaManifestFailureCallback failure_callback)
{
    clear_manifest_output(manifest);
    if (!manifest || !response || response.size() <= 1) {
        notify_manifest_failure(failure_callback);
        return false;
    }
    if (!ota_manifest_source_valid(source)) {
        ESP_LOGW(TAG,
                 MANIFEST_SOURCE_SKIPPED_FORMAT,
                 ota_manifest_source_name_or_unknown(source.name));
        return false;
    }
    response.data()[0] = '\0';
    esp_err_t err = http_get_text(source.url, response.data(), response.size());
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 MANIFEST_FETCH_FAILED_FORMAT,
                 ota_manifest_source_name_or_unknown(source.name),
                 esp_err_to_name(err));
        return false;
    }
    if (!parse_manifest_with_log(response.data(), manifest)) {
        ESP_LOGW(TAG,
                 MANIFEST_PARSE_FAILED_FORMAT,
                 ota_manifest_source_name_or_unknown(source.name));
        return false;
    }
    ESP_LOGI(TAG,
             MANIFEST_LOADED_FORMAT,
             ota_manifest_source_name_or_unknown(source.name),
             manifest->version);
    return true;
}

void store_manifest_source_name(char *out, size_t out_len, const char *name)
{
    if (!app_text::output_buffer_available(out, out_len)) {
        return;
    }
    strlcpy(out, ota_manifest_source_name_or_unknown(name), out_len);
}
} // namespace

void ota_manifest_load_cached(OtaManifest *manifest)
{
    if (!manifest) {
        return;
    }
    strlcpy(manifest->version, s_cached_manifest.version, sizeof(manifest->version));
    strlcpy(manifest->url, s_cached_manifest.url, sizeof(manifest->url));
    strlcpy(manifest->sha256, s_cached_manifest.sha256, sizeof(manifest->sha256));
    manifest->size = s_cached_manifest.size;
}

void ota_manifest_store_cached(const OtaManifest &manifest)
{
    strlcpy(s_cached_manifest.version, manifest.version, sizeof(s_cached_manifest.version));
    strlcpy(s_cached_manifest.url, manifest.url, sizeof(s_cached_manifest.url));
    strlcpy(s_cached_manifest.sha256, manifest.sha256, sizeof(s_cached_manifest.sha256));
    s_cached_manifest.size = manifest.size;
}

bool ota_manifest_fetch(OtaManifest *manifest,
                        char *source_name,
                        size_t source_name_len,
                        OtaManifestFailureCallback failure_callback)
{
    clear_manifest_output(manifest);
    clear_text_output(source_name, source_name_len);
    if (!manifest) {
        notify_manifest_failure(failure_callback);
        return false;
    }
    ScopedHeapBuffer<char> response(kManifestResponseBufferSize,
                                    HeapBufferInit::kCString,
                                    HeapBufferStorage::kPsramPreferred);
    if (!response) {
        ESP_LOGW(TAG, "%s", kManifestResponseAllocFailedLog);
        notify_manifest_failure(failure_callback);
        return false;
    }
    char custom_url[kOtaUrlLen] = {};
    if (custom_assets_read_ota_manifest_url(custom_url, sizeof(custom_url))) {
        OtaManifestSource custom_source = {kManifestSourceCustom, custom_url};
        if (fetch_manifest_from_source(custom_source,
                                       manifest,
                                       response,
                                       failure_callback)) {
            store_manifest_source_name(source_name, source_name_len, custom_source.name);
            return true;
        }
    }
    for (const OtaManifestSource &source : kBuiltInManifestSources) {
        if (fetch_manifest_from_source(source,
                                       manifest,
                                       response,
                                       failure_callback)) {
            store_manifest_source_name(source_name, source_name_len, source.name);
            return true;
        }
    }
    clear_manifest_output(manifest);
    notify_manifest_failure(failure_callback);
    return false;
}

bool ota_manifest_fetch_backup_for_install(const OtaManifest &current,
                                           OtaManifest *backup,
                                           OtaManifestFailureCallback failure_callback)
{
    if (!backup || backup == &current) {
        return false;
    }
    clear_manifest_output(backup);
    if (current.version[0] == '\0' ||
        !ota_valid_sha256_string(current.sha256)) {
        return false;
    }
    ScopedHeapBuffer<char> response(kManifestResponseBufferSize,
                                    HeapBufferInit::kCString,
                                    HeapBufferStorage::kPsramPreferred);
    if (!response) {
        ESP_LOGW(TAG, "%s", kManifestResponseAllocFailedLog);
        notify_manifest_failure(failure_callback);
        return false;
    }
    for (const OtaManifestSource &backup_source : kBuiltInManifestSources) {
        if (!fetch_manifest_from_source(backup_source,
                                        backup,
                                        response,
                                        failure_callback)) {
            continue;
        }
        if (strcmp(backup->url, current.url) == 0) {
            continue;
        }
        if (!ota_backup_manifest_metadata_matches(current.version,
                                                  current.sha256,
                                                  current.size,
                                                  backup->version,
                                                  backup->sha256,
                                                  backup->size)) {
            ESP_LOGW(TAG,
                     BACKUP_MANIFEST_MISMATCH_FORMAT,
                     current.version,
                     backup->version);
            continue;
        }
        return true;
    }
    clear_manifest_output(backup);
    return false;
}
