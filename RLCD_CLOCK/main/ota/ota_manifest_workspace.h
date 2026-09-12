// 管理 OTA 检查与备用下载共用的双 manifest PSRAM 工作区。
#pragma once

#include "ota_manifest_parser.h"

class OtaManifestWorkspaceGuard {
public:
    OtaManifestWorkspaceGuard();
    ~OtaManifestWorkspaceGuard();

    OtaManifestWorkspaceGuard(const OtaManifestWorkspaceGuard &) = delete;
    OtaManifestWorkspaceGuard &operator=(const OtaManifestWorkspaceGuard &) = delete;

    OtaManifest &primary();
    OtaManifest &backup();
    void clear();

private:
    OtaManifest *primary_;
    OtaManifest *backup_;
};
