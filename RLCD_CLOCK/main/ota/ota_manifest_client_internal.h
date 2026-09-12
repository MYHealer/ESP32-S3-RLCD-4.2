// 声明仅供串行 OTA 任务维护的运行态 manifest 缓存接口。
#pragma once

#include "ota_manifest_parser.h"

void ota_manifest_load_cached(OtaManifest *manifest);
void ota_manifest_store_cached(const OtaManifest &manifest);
