// 私有持有应用事件组控制块和句柄，并保护初始化前的调用。
#include "app_event_group_internal.h"

namespace {
StaticEventGroup_t s_event_group_storage = {};
EventGroupHandle_t s_event_group = nullptr;
} // namespace

bool app_event_group_init()
{
    if (s_event_group) {
        return true;
    }
    s_event_group = xEventGroupCreateStatic(&s_event_group_storage);
    return s_event_group != nullptr;
}

void app_event_group_release()
{
    if (!s_event_group) {
        return;
    }
    vEventGroupDelete(s_event_group);
    s_event_group = nullptr;
}

bool app_event_group_ready()
{
    return s_event_group != nullptr;
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    return s_event_group ? xEventGroupSetBits(s_event_group, bits) : 0;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    return s_event_group ? xEventGroupClearBits(s_event_group, bits) : 0;
}

EventBits_t app_event_group_get_bits()
{
    return s_event_group ? xEventGroupGetBits(s_event_group) : 0;
}

EventBits_t app_event_group_wait_bits(EventBits_t bits,
                                      BaseType_t clear_on_exit,
                                      BaseType_t wait_for_all,
                                      TickType_t timeout)
{
    return s_event_group
               ? xEventGroupWaitBits(s_event_group,
                                     bits,
                                     clear_on_exit,
                                     wait_for_all,
                                     timeout)
               : 0;
}
