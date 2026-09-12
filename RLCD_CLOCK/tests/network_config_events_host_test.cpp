// 验证网络配置 EventGroup 适配层的空句柄保护、日志回退和位操作转发。
#include "network_config_internal.h"

#include "app_event_group.h"

#include <assert.h>
#include <string.h>

namespace {
bool g_event_group_ready = false;
int g_clear_calls = 0;
int g_set_calls = 0;
EventBits_t g_last_bits = 0;
const char *g_last_action = nullptr;
const char *g_last_reason = nullptr;
} // namespace

void record_config_event_log(const char *action, const char *reason)
{
    g_last_action = action;
    g_last_reason = reason;
}

bool app_event_group_ready()
{
    return g_event_group_ready;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    ++g_clear_calls;
    g_last_bits = bits;
    return bits;
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    ++g_set_calls;
    g_last_bits = bits;
    return bits;
}

int main()
{
    clear_config_event_bits(0x12, nullptr);
    assert(g_clear_calls == 0);
    assert(strcmp(g_last_action, "clear") == 0);
    assert(strcmp(g_last_reason, "config") == 0);

    set_config_event_bits(0x34, "provisioning save");
    assert(g_set_calls == 0);
    assert(strcmp(g_last_action, "set") == 0);
    assert(strcmp(g_last_reason, "provisioning save") == 0);

    g_event_group_ready = true;
    clear_config_event_bits(0x56, "reset");
    assert(g_clear_calls == 1);
    assert(g_last_bits == 0x56);

    set_config_event_bits(0x78, "sync");
    assert(g_set_calls == 1);
    assert(g_last_bits == 0x78);
    return 0;
}
