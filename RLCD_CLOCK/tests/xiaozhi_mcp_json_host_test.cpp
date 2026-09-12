// 验证小智 MCP 共用字段 helper 的成功、判空和失败释放语义。
#include "xiaozhi_mcp_json.h"
#include "xiaozhi_mcp_schema.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
size_t s_live_allocations = 0;
size_t s_allocation_attempts = 0;
size_t s_fail_after = static_cast<size_t>(-1);

void *tracking_malloc(size_t size)
{
    if (s_allocation_attempts++ == s_fail_after) {
        return nullptr;
    }
    void *memory = malloc(size);
    if (memory) {
        ++s_live_allocations;
    }
    return memory;
}

void tracking_free(void *memory)
{
    if (memory) {
        assert(s_live_allocations > 0);
        --s_live_allocations;
    }
    free(memory);
}

template <typename Builder>
void expect_builder_releases_every_failed_allocation(Builder builder)
{
    bool reached_success = false;
    for (size_t fail_after = 0; fail_after < 512; ++fail_after) {
        assert(s_live_allocations == 0);
        s_allocation_attempts = 0;
        s_fail_after = fail_after;
        cJSON *result = builder();
        s_fail_after = static_cast<size_t>(-1);
        reached_success = result != nullptr;
        cJSON_Delete(result);
        if (s_live_allocations != 0) {
            fprintf(stderr,
                    "cJSON builder leaked at allocation %zu: live=%zu\n",
                    fail_after,
                    s_live_allocations);
        }
        assert(s_live_allocations == 0);
        if (reached_success) {
            break;
        }
    }
    assert(reached_success);
}
} // namespace

int main()
{
    cJSON_Hooks hooks = {tracking_malloc, tracking_free};
    cJSON_InitHooks(&hooks);

    cJSON *object = cJSON_CreateObject();
    assert(object != nullptr);
    assert(xiaozhi_mcp_json::add_string(object, "name", "value"));
    const cJSON *value = cJSON_GetObjectItem(object, "name");
    assert(cJSON_IsString(value));
    assert(strcmp(value->valuestring, "value") == 0);

    assert(!xiaozhi_mcp_json::add_string(nullptr, "name", "value"));
    assert(!xiaozhi_mcp_json::add_string(object, nullptr, "value"));
    assert(!xiaozhi_mcp_json::add_string(object, "name", nullptr));

    cJSON *owned_object_value = cJSON_CreateString("owned");
    assert(owned_object_value != nullptr);
    assert(xiaozhi_mcp_json::add_owned_item_to_object(
        object, "owned", owned_object_value));

    size_t live_before_failed_object_add = s_live_allocations;
    cJSON *rejected_object_value = cJSON_CreateString("rejected");
    assert(rejected_object_value != nullptr);
    assert(s_live_allocations > live_before_failed_object_add);
    assert(!xiaozhi_mcp_json::add_owned_item_to_object(
        nullptr, "rejected", rejected_object_value));
    assert(s_live_allocations == live_before_failed_object_add);

    cJSON *array = cJSON_CreateArray();
    assert(array != nullptr);
    assert(xiaozhi_mcp_json::add_owned_item_to_array(
        array, cJSON_CreateString("owned")));
    size_t live_before_failed_array_add = s_live_allocations;
    cJSON *rejected_array_value = cJSON_CreateString("rejected");
    assert(rejected_array_value != nullptr);
    assert(!xiaozhi_mcp_json::add_owned_item_to_array(
        nullptr, rejected_array_value));
    assert(s_live_allocations == live_before_failed_array_add);

    assert(!xiaozhi_mcp_json::add_owned_item_to_object(object, "empty", nullptr));
    assert(!xiaozhi_mcp_json::add_owned_item_to_array(array, nullptr));

    cJSON_Delete(array);
    cJSON_Delete(object);
    assert(s_live_allocations == 0);

    expect_builder_releases_every_failed_allocation([]() {
        return xiaozhi_mcp_schema::create_initialize_result("v-test");
    });
    expect_builder_releases_every_failed_allocation([]() {
        return xiaozhi_mcp_schema::create_tools_list_result(
            true, true, true, true, true);
    });

    cJSON_InitHooks(nullptr);
    return 0;
}
