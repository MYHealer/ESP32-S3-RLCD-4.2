// 实现 OTA 双 manifest 工作区的构造、复用和敏感数据清理。
#include "ota_manifest_workspace.h"

#include <esp_attr.h>

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

namespace {

struct OtaManifestStorage {
    alignas(OtaManifest) uint8_t bytes[sizeof(OtaManifest)];
};

struct OtaManifestWorkspace {
    OtaManifestStorage primary;
    OtaManifestStorage backup;
};

// OTA check/install requests are serialized by ota_task. Keep both manifests
// out of its stack while preserving separate primary and backup snapshots.
EXT_RAM_BSS_ATTR OtaManifestWorkspace s_ota_manifest_workspace;

static_assert(std::is_trivially_default_constructible<OtaManifestWorkspace>::value,
              "OTA manifest raw workspace must not require static construction");
static_assert(std::is_trivially_destructible<OtaManifest>::value,
              "OTA manifest workspace clears storage without destructor side effects");
static_assert(sizeof(OtaManifestWorkspace) >= sizeof(OtaManifest) * 2,
              "OTA manifest workspace must hold independent primary and backup objects");

} // namespace

OtaManifestWorkspaceGuard::OtaManifestWorkspaceGuard()
    : primary_(new (s_ota_manifest_workspace.primary.bytes) OtaManifest()),
      backup_(new (s_ota_manifest_workspace.backup.bytes) OtaManifest())
{
}

OtaManifestWorkspaceGuard::~OtaManifestWorkspaceGuard()
{
    clear();
}

OtaManifest &OtaManifestWorkspaceGuard::primary()
{
    return *primary_;
}

OtaManifest &OtaManifestWorkspaceGuard::backup()
{
    return *backup_;
}

void OtaManifestWorkspaceGuard::clear()
{
    if (!primary_) {
        return;
    }
    primary_->~OtaManifest();
    backup_->~OtaManifest();
    volatile uint8_t *bytes =
        reinterpret_cast<volatile uint8_t *>(&s_ota_manifest_workspace);
    for (size_t remaining = sizeof(s_ota_manifest_workspace);
         remaining > 0;
         --remaining) {
        *bytes++ = 0;
    }
    primary_ = nullptr;
    backup_ = nullptr;
}
