// 验证 OTA manifest 来源名称、占位地址与空值过滤规则。
#include "ota_manifest_client.h"

#include <assert.h>
#include <string.h>

int main()
{
    OtaManifestSource valid = {"GitHub", "https://ota.example.com/latest.json"};
    assert(ota_manifest_source_valid(valid));

    OtaManifestSource missing_name = {nullptr, valid.url};
    OtaManifestSource empty_name = {"", valid.url};
    OtaManifestSource missing_url = {valid.name, nullptr};
    OtaManifestSource empty_url = {valid.name, ""};
    OtaManifestSource placeholder = {valid.name, "https://example.invalid/latest.json"};
    assert(!ota_manifest_source_valid(missing_name));
    assert(!ota_manifest_source_valid(empty_name));
    assert(!ota_manifest_source_valid(missing_url));
    assert(!ota_manifest_source_valid(empty_url));
    assert(!ota_manifest_source_valid(placeholder));

    assert(strcmp(ota_manifest_source_name_or_unknown("Custom"), "Custom") == 0);
    assert(strcmp(ota_manifest_source_name_or_unknown("Gitee"), "Gitee") == 0);
    assert(strcmp(ota_manifest_source_name_or_unknown(nullptr), "unknown") == 0);
    assert(strcmp(ota_manifest_source_name_or_unknown(""), "unknown") == 0);
    assert(kOtaManifestSourceNameLen > strlen("unknown"));
    return 0;
}
