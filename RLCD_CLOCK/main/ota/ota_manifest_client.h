// 声明 OTA manifest 来源策略、运行态缓存和主备清单获取接口。
#pragma once

#include "ota_manifest_parser.h"

#include <stddef.h>
#include <string.h>

inline constexpr size_t kOtaManifestSourceNameLen = 16;
inline constexpr const char *kOtaUnknownManifestSource = "unknown";
inline constexpr const char *kOtaPlaceholderManifestHost = "example.invalid";

struct OtaManifestSource {
    const char *name = nullptr;
    const char *url = nullptr;
};

using OtaManifestFailureCallback = void (*)();

inline bool ota_manifest_source_valid(const OtaManifestSource &source)
{
    return source.name && source.name[0] != '\0' &&
           source.url && source.url[0] != '\0' &&
           strstr(source.url, kOtaPlaceholderManifestHost) == nullptr;
}

inline const char *ota_manifest_source_name_or_unknown(const char *name)
{
    return name && name[0] != '\0' ? name : kOtaUnknownManifestSource;
}

// manifest and source_name are cleared before work and remain empty on failure.
bool ota_manifest_fetch(OtaManifest *manifest,
                        char *source_name,
                        size_t source_name_len,
                        OtaManifestFailureCallback failure_callback);
// backup must not alias current; a non-aliased output remains empty on failure.
bool ota_manifest_fetch_backup_for_install(const OtaManifest &current,
                                           OtaManifest *backup,
                                           OtaManifestFailureCallback failure_callback);
