// 验证应用事件组所有者的生命周期、空句柄保护和参数透传。
#include "app_event_group_internal.h"
#include "network_runtime_events.h"

#include <assert.h>

static_assert(kWifiConnectedBit == (1U << 0));
static_assert(kTimeSyncedBit == (1U << 1));
static_assert(kWeatherReadyBit == (1U << 2));
static_assert(kProvisioningSyncBit == (1U << 3));
static_assert(kManualNtpSyncBit == (1U << 4));
static_assert(kManualWeatherSyncBit == (1U << 5));
static_assert(kOtaCheckBit == (1U << 6));
static_assert(kOtaInstallBit == (1U << 7));
static_assert(kManualSayingSyncBit == (1U << 8));
static_assert(kBootSyncDoneBit == (1U << 9));
static_assert(kBootAnimDoneBit == (1U << 10));
static_assert(kNetworkDiagBit == (1U << 11));
static_assert(kNetworkStateChangedBit == (1U << 12));
static_assert(kSetupPortalStartBit == (1U << 13));
static_assert(kProvisioningFeedbackBit == (1U << 14));
static_assert(kVisibleWeatherSyncBit == (1U << 15));
static_assert(kVisibleSayingSyncBit == (1U << 16));
static_assert(kNtpSyncCompletedBit == (1U << 17));
static_assert(kXiaozhiPageStateChangedBit == (1U << 18));
static_assert((kWifiConnectedBit | kTimeSyncedBit | kWeatherReadyBit |
               kProvisioningSyncBit | kManualNtpSyncBit |
               kManualWeatherSyncBit | kOtaCheckBit | kOtaInstallBit |
               kManualSayingSyncBit | kBootSyncDoneBit | kBootAnimDoneBit |
               kNetworkDiagBit | kNetworkStateChangedBit |
               kSetupPortalStartBit | kProvisioningFeedbackBit |
               kVisibleWeatherSyncBit | kVisibleSayingSyncBit |
               kNtpSyncCompletedBit | kXiaozhiPageStateChangedBit) ==
              ((1U << 19) - 1U));

namespace {
bool s_create_allowed = true;
int s_create_calls = 0;
int s_delete_calls = 0;
int s_set_calls = 0;
int s_clear_calls = 0;
int s_get_calls = 0;
int s_wait_calls = 0;
EventBits_t s_bits = 0;
EventBits_t s_last_wait_bits = 0;
BaseType_t s_last_clear_on_exit = 0;
BaseType_t s_last_wait_for_all = 0;
TickType_t s_last_timeout = 0;
EventGroupHandle_t s_last_handle = nullptr;
} // namespace

EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *storage)
{
    ++s_create_calls;
    return s_create_allowed ? storage : nullptr;
}

void vEventGroupDelete(EventGroupHandle_t group)
{
    ++s_delete_calls;
    s_last_handle = group;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
    ++s_set_calls;
    s_last_handle = group;
    s_bits |= bits;
    return s_bits;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits)
{
    ++s_clear_calls;
    s_last_handle = group;
    EventBits_t previous = s_bits;
    s_bits &= ~bits;
    return previous;
}

EventBits_t xEventGroupGetBits(EventGroupHandle_t group)
{
    ++s_get_calls;
    s_last_handle = group;
    return s_bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t group,
                                EventBits_t bits,
                                BaseType_t clear_on_exit,
                                BaseType_t wait_for_all,
                                TickType_t timeout)
{
    ++s_wait_calls;
    s_last_handle = group;
    s_last_wait_bits = bits;
    s_last_clear_on_exit = clear_on_exit;
    s_last_wait_for_all = wait_for_all;
    s_last_timeout = timeout;
    EventBits_t observed = s_bits;
    if (clear_on_exit) {
        s_bits &= ~bits;
    }
    return observed;
}

int main()
{
    assert(!app_event_group_ready());
    assert(app_event_group_set_bits(0x01) == 0);
    assert(app_event_group_clear_bits(0x01) == 0);
    assert(app_event_group_get_bits() == 0);
    assert(app_event_group_wait_bits(0x01, pdTRUE, pdFALSE, 10) == 0);
    assert(s_set_calls == 0 && s_clear_calls == 0 && s_get_calls == 0 && s_wait_calls == 0);

    assert(app_event_group_init());
    assert(app_event_group_ready());
    assert(s_create_calls == 1);
    EventGroupHandle_t created_handle = s_last_handle;
    assert(app_event_group_init());
    assert(s_create_calls == 1);

    assert(app_event_group_set_bits(0x05) == 0x05);
    created_handle = s_last_handle;
    assert(created_handle != nullptr);
    assert(app_event_group_clear_bits(0x01) == 0x05);
    assert(app_event_group_get_bits() == 0x04);
    assert(app_event_group_wait_bits(0x04, pdTRUE, pdFALSE, 123) == 0x04);
    assert(s_last_handle == created_handle);
    assert(s_last_wait_bits == 0x04);
    assert(s_last_clear_on_exit == pdTRUE);
    assert(s_last_wait_for_all == pdFALSE);
    assert(s_last_timeout == 123);
    assert(s_bits == 0);

    const int clear_calls_before_empty_cancel = s_clear_calls;
    assert(!cancel_pending_network_sync_requests(0));
    assert(s_clear_calls == clear_calls_before_empty_cancel);

    assert(app_event_group_set_bits(kVisibleWeatherSyncBit |
                                    kVisibleSayingSyncBit) ==
           (kVisibleWeatherSyncBit | kVisibleSayingSyncBit));
    const int set_calls_before_cancel = s_set_calls;
    assert(cancel_pending_network_sync_requests(kVisibleWeatherSyncBit));
    assert((s_bits & kVisibleWeatherSyncBit) == 0);
    assert((s_bits & kVisibleSayingSyncBit) != 0);
    assert((s_bits & kNetworkStateChangedBit) != 0);
    assert(s_set_calls == set_calls_before_cancel + 1);

    assert(app_event_group_clear_bits(kNetworkStateChangedBit) != 0);
    const int set_calls_before_settled_cancel = s_set_calls;
    assert(!cancel_pending_network_sync_requests(kVisibleWeatherSyncBit));
    assert((s_bits & kNetworkStateChangedBit) == 0);
    assert(s_set_calls == set_calls_before_settled_cancel);
    assert(cancel_pending_network_sync_requests(kVisibleSayingSyncBit));
    assert((s_bits & kVisibleSayingSyncBit) == 0);
    assert((s_bits & kNetworkStateChangedBit) != 0);
    app_event_group_clear_bits(kNetworkStateChangedBit);

    app_event_group_release();
    assert(!app_event_group_ready());
    assert(s_delete_calls == 1);
    assert(s_last_handle == created_handle);
    app_event_group_release();
    assert(s_delete_calls == 1);

    s_create_allowed = false;
    assert(!app_event_group_init());
    assert(!app_event_group_ready());
    assert(s_create_calls == 2);

    s_create_allowed = true;
    assert(app_event_group_init());
    assert(app_event_group_ready());
    assert(s_create_calls == 3);
    app_event_group_release();
    assert(s_delete_calls == 2);
    return 0;
}
