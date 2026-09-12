// 声明小智 MCP 响应与 Schema 共用的 cJSON 构建 helper。
#pragma once

#include "cJSON.h"

namespace xiaozhi_mcp_json {

bool add_string(cJSON *object, const char *name, const char *value);
// 接管 item；挂接失败时立即释放，成功后所有权转移给父节点。
bool add_owned_item_to_object(cJSON *object, const char *name, cJSON *item);
bool add_owned_item_to_array(cJSON *array, cJSON *item);

} // namespace xiaozhi_mcp_json
