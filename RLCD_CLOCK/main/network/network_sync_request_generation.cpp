// 实现用户触发联网请求的独立代次和条件结算。
#include "network_sync_request_generation_internal.h"

#include "app_event_group.h"
#include "scoped_semaphore_lock.h"

#include <atomic>

namespace {
enum TrackedRequestIndex {
    kManualNtpRequestIndex = 0,
    kManualWeatherRequestIndex,
    kManualSayingRequestIndex,
    kDiagnosticsRequestIndex,
    kTrackedRequestCount,
};

struct TrackedNetworkRequest {
    EventBits_t bit;
    std::atomic<uint32_t> generation;
};

TrackedNetworkRequest s_tracked_requests[kTrackedRequestCount] = {
    {kManualNtpSyncBit, 0},
    {kManualWeatherSyncBit, 0},
    {kManualSayingSyncBit, 0},
    {kNetworkDiagBit, 0},
};
StaticTaskMutex s_request_lifecycle_mutex;
static_assert(kTrackedRequestCount == 4,
              "tracked request snapshot fields must match the table");

TrackedNetworkRequest *tracked_request_for_single_bit(EventBits_t bit)
{
    if (bit == 0 || (bit & (bit - 1)) != 0) {
        return nullptr;
    }
    for (TrackedNetworkRequest &request : s_tracked_requests) {
        if (request.bit == bit) {
            return &request;
        }
    }
    return nullptr;
}

uint32_t advance_generation(TrackedNetworkRequest &request)
{
    uint32_t next =
        request.generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (next == 0) {
        next =
            request.generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    return next;
}
} // namespace

bool init_network_sync_request_generation()
{
    return s_request_lifecycle_mutex.init();
}

NetworkSyncRequestGenerationSnapshot
network_sync_request_generation_snapshot()
{
    NetworkSyncRequestGenerationSnapshot snapshot;
    snapshot.manual_ntp =
        s_tracked_requests[kManualNtpRequestIndex].generation.load(
            std::memory_order_acquire);
    snapshot.manual_weather =
        s_tracked_requests[kManualWeatherRequestIndex].generation.load(
            std::memory_order_acquire);
    snapshot.manual_saying =
        s_tracked_requests[kManualSayingRequestIndex].generation.load(
            std::memory_order_acquire);
    snapshot.diagnostics =
        s_tracked_requests[kDiagnosticsRequestIndex].generation.load(
            std::memory_order_acquire);
    return snapshot;
}

uint32_t publish_network_sync_request(EventBits_t request_bit)
{
    TrackedNetworkRequest *request =
        tracked_request_for_single_bit(request_bit);
    if (!request) {
        return 0;
    }
    ScopedSemaphoreLock lock(s_request_lifecycle_mutex);
    if (!lock) {
        return 0;
    }
    const uint32_t generation = advance_generation(*request);
    app_event_group_set_bits(request_bit | kNetworkStateChangedBit);
    return generation;
}

bool network_sync_request_is_current(EventBits_t request_bit,
                                     uint32_t expected_generation)
{
    TrackedNetworkRequest *request =
        tracked_request_for_single_bit(request_bit);
    if (!request || expected_generation == 0 ||
        !s_request_lifecycle_mutex.handle()) {
        return false;
    }
    ScopedSemaphoreLock lock(s_request_lifecycle_mutex);
    if (!lock) {
        return false;
    }
    return request->generation.load(std::memory_order_acquire) ==
               expected_generation &&
           (app_event_group_get_bits() & request_bit) != 0;
}

bool retire_network_sync_request(EventBits_t request_bit,
                                 uint32_t expected_generation)
{
    TrackedNetworkRequest *request =
        tracked_request_for_single_bit(request_bit);
    if (!request || expected_generation == 0 ||
        !s_request_lifecycle_mutex.handle()) {
        return false;
    }
    ScopedSemaphoreLock lock(s_request_lifecycle_mutex);
    if (!lock ||
        request->generation.load(std::memory_order_acquire) !=
            expected_generation) {
        return false;
    }

    const EventBits_t previous_bits =
        app_event_group_clear_bits(request_bit);
    if (request->generation.load(std::memory_order_acquire) !=
        expected_generation) {
        // A producer advanced the generation between the first check and the
        // clear. Restore its level-triggered event so the network task sees it.
        app_event_group_set_bits(request_bit);
        return false;
    }
    return (previous_bits & request_bit) != 0;
}

void invalidate_network_sync_requests(EventBits_t request_bits)
{
    ScopedSemaphoreLock lock(s_request_lifecycle_mutex);
    if (!lock) {
        return;
    }
    for (TrackedNetworkRequest &request : s_tracked_requests) {
        if ((request_bits & request.bit) != 0) {
            (void)advance_generation(request);
        }
    }
    app_event_group_clear_bits(request_bits);
}
