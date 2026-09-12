// 实现小智 MCP 响应与 Schema 共用的 cJSON 字符串字段写入。
#include "xiaozhi_mcp_json.h"

namespace xiaozhi_mcp_json {

bool add_string(cJSON *object, const char *name, const char *value)
{
    return object && name && value && cJSON_AddStringToObject(object, name, value) != nullptr;
}

bool add_owned_item_to_object(cJSON *object, const char *name, cJSON *item)
{
    if (!item) {
        return false;
    }
    if (!object || !name || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool add_owned_item_to_array(cJSON *array, cJSON *item)
{
    if (!item) {
        return false;
    }
    if (!array || !cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

} // namespace xiaozhi_mcp_json
