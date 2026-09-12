// 集中维护配网页会话状态、断线原因、AP 名称和本地 IP 完整快照。
#include "wifi_portal_state_internal.h"

#include "app_event_group.h"
#include "scoped_semaphore_lock.h"

#include <atomic>
#include <esp_attr.h>
#include <freertos/task.h>
#include <string.h>

namespace {
std::atomic<bool> s_setup_portal_active{false};
std::atomic<int> s_last_wifi_disconnect_reason{0};
constexpr uint32_t kSaveResultMask = 0x7u;
constexpr uint32_t kSaveFeedbackSeenBit = 1u << 3;
constexpr uint32_t kSaveGenerationShift = 4;
constexpr uint32_t kSaveGenerationMask =
    UINT32_MAX >> kSaveGenerationShift;
std::atomic<uint32_t> s_save_state{0};
std::atomic<uint8_t> s_setup_ap_client_count{0};
std::atomic<bool> s_setup_ap_channel_transition_active{false};
StaticTaskMutex s_portal_text_mutex;
EXT_RAM_BSS_ATTR char s_setup_ap_ssid[kWifiSetupApSsidTextLen] = {};
EXT_RAM_BSS_ATTR char s_station_ip[kWifiStationIpTextLen] = {};

static_assert(sizeof(s_setup_ap_ssid) == kWifiSetupApSsidTextLen,
              "setup AP SSID storage must match the public text contract");
static_assert(sizeof(s_station_ip) == kWifiStationIpTextLen,
              "station IP storage must match the public text contract");
static_assert(static_cast<uint32_t>(WifiPortalSaveResult::kSuccess) <=
                  kSaveResultMask,
              "portal save result must fit the packed state");

constexpr uint32_t pack_save_state(WifiPortalSaveResult result,
                                   bool feedback_seen,
                                   uint32_t generation)
{
    return ((generation & kSaveGenerationMask) << kSaveGenerationShift) |
           (feedback_seen ? kSaveFeedbackSeenBit : 0u) |
           (static_cast<uint32_t>(result) & kSaveResultMask);
}

constexpr WifiPortalSaveSnapshot unpack_save_state(uint32_t state)
{
    return {
        static_cast<WifiPortalSaveResult>(state & kSaveResultMask),
        (state & kSaveFeedbackSeenBit) != 0,
        state >> kSaveGenerationShift,
    };
}

WifiPortalSaveSnapshot begin_save_attempt(bool notify_waiters)
{
    uint32_t current = s_save_state.load(std::memory_order_acquire);
    for (;;) {
        const WifiPortalSaveSnapshot previous = unpack_save_state(current);
        const uint32_t next_generation =
            (previous.generation + 1u) & kSaveGenerationMask;
        const uint32_t next =
            pack_save_state(WifiPortalSaveResult::kNone,
                            false,
                            next_generation);
        if (s_save_state.compare_exchange_weak(
                current,
                next,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            if (notify_waiters && app_event_group_ready()) {
                // A locally rejected save has no provisioning request bit.
                // Publish this edge so an older result wait still observes the
                // generation change immediately.
                app_event_group_set_bits(kProvisioningFeedbackBit);
            }
            return unpack_save_state(next);
        }
        taskYIELD();
    }
}

template <size_t N>
bool portal_text_snapshot(const char (&source)[N], char *out, size_t out_len)
{
    if (!out || out_len < N) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return false;
    }
    ScopedSemaphoreLock lock(s_portal_text_mutex.handle());
    if (!lock) {
        out[0] = '\0';
        return false;
    }
    memcpy(out, source, N);
    return out[0] != '\0';
}

template <size_t N>
void portal_text_store(char (&target)[N], const char *value)
{
    char replacement[N] = {};
    strlcpy(replacement, value ? value : "", sizeof(replacement));
    ScopedSemaphoreLock lock(s_portal_text_mutex.handle());
    if (!lock) {
        return;
    }
    memcpy(target, replacement, N);
}
} // namespace

bool wifi_portal_state_init()
{
    return s_portal_text_mutex.init();
}

bool setup_portal_active_load()
{
    return s_setup_portal_active.load(std::memory_order_acquire);
}

void setup_portal_active_store(bool active)
{
    const bool was_active =
        s_setup_portal_active.exchange(active, std::memory_order_acq_rel);
    if (was_active && !active && app_event_group_ready()) {
        // A stopped portal must wake both the result waiter and the network
        // scheduler. The scheduler can then block indefinitely while the AP is
        // idle instead of polling only to discover this transition.
        app_event_group_set_bits(kProvisioningFeedbackBit |
                                 kNetworkStateChangedBit);
    }
}

int wifi_last_disconnect_reason()
{
    return s_last_wifi_disconnect_reason.load(std::memory_order_acquire);
}

void record_wifi_disconnect_reason(int reason)
{
    s_last_wifi_disconnect_reason.store(reason, std::memory_order_release);
}

void clear_wifi_last_disconnect_reason()
{
    record_wifi_disconnect_reason(0);
}

bool wifi_setup_ap_ssid_snapshot(char *out, size_t out_len)
{
    return portal_text_snapshot(s_setup_ap_ssid, out, out_len);
}

void wifi_setup_ap_ssid_store(const char *ssid)
{
    portal_text_store(s_setup_ap_ssid, ssid);
}

bool wifi_station_ip_snapshot(char *out, size_t out_len)
{
    return portal_text_snapshot(s_station_ip, out, out_len);
}

void wifi_station_ip_store(const char *ip_text)
{
    portal_text_store(s_station_ip, ip_text);
}

void clear_wifi_station_ip()
{
    wifi_station_ip_store("");
}

WifiPortalSaveSnapshot wifi_portal_save_snapshot_load()
{
    return unpack_save_state(
        s_save_state.load(std::memory_order_acquire));
}

WifiPortalSaveResult wifi_portal_save_result_load()
{
    return wifi_portal_save_snapshot_load().result;
}

WifiPortalSaveSnapshot wifi_portal_begin_save_attempt()
{
    return begin_save_attempt(true);
}

void wifi_portal_save_result_store(WifiPortalSaveResult result)
{
    uint32_t current = s_save_state.load(std::memory_order_acquire);
    for (;;) {
        const WifiPortalSaveSnapshot previous = unpack_save_state(current);
        const uint32_t next =
            pack_save_state(result, false, previous.generation);
        if (s_save_state.compare_exchange_weak(
                current,
                next,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;
        }
        taskYIELD();
    }
}

bool wifi_portal_save_result_store_if_generation(
    uint32_t generation,
    WifiPortalSaveResult result,
    WifiPortalSaveSnapshot *updated)
{
    uint32_t current = s_save_state.load(std::memory_order_acquire);
    for (;;) {
        const WifiPortalSaveSnapshot previous = unpack_save_state(current);
        if (previous.generation !=
            (generation & kSaveGenerationMask)) {
            return false;
        }
        const uint32_t next =
            pack_save_state(result, false, previous.generation);
        if (s_save_state.compare_exchange_weak(
                current,
                next,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            if (updated) {
                *updated = unpack_save_state(next);
            }
            return true;
        }
        taskYIELD();
    }
}

bool wifi_portal_save_feedback_seen_load()
{
    return wifi_portal_save_snapshot_load().feedback_seen;
}

void wifi_portal_save_feedback_seen_store(bool seen)
{
    if (seen) {
        s_save_state.fetch_or(kSaveFeedbackSeenBit,
                              std::memory_order_acq_rel);
    } else {
        s_save_state.fetch_and(~kSaveFeedbackSeenBit,
                               std::memory_order_acq_rel);
    }
    if (!app_event_group_ready()) {
        return;
    }
    if (seen) {
        app_event_group_set_bits(kProvisioningFeedbackBit);
    } else {
        app_event_group_clear_bits(kProvisioningFeedbackBit);
    }
}

bool wifi_portal_mark_save_feedback_seen(
    const WifiPortalSaveSnapshot &snapshot)
{
    if (snapshot.result != WifiPortalSaveResult::kSuccess) {
        return false;
    }
    if (snapshot.feedback_seen) {
        return wifi_portal_save_snapshot_load().generation ==
               snapshot.generation;
    }

    uint32_t expected = pack_save_state(snapshot.result,
                                        false,
                                        snapshot.generation);
    const uint32_t desired = expected | kSaveFeedbackSeenBit;
    if (!s_save_state.compare_exchange_strong(
            expected,
            desired,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return false;
    }
    if (app_event_group_ready()) {
        app_event_group_set_bits(kProvisioningFeedbackBit);
    }
    return true;
}

void wifi_portal_session_reset()
{
    s_setup_ap_client_count.store(0, std::memory_order_release);
    s_setup_ap_channel_transition_active.store(false,
                                               std::memory_order_release);
    begin_save_attempt(false);
    wifi_portal_save_feedback_seen_store(false);
}

void wifi_portal_ap_channel_transition_begin()
{
    s_setup_ap_channel_transition_active.store(true,
                                               std::memory_order_release);
}

void wifi_portal_ap_channel_transition_end()
{
    s_setup_ap_channel_transition_active.store(false,
                                               std::memory_order_release);
}

uint8_t wifi_portal_ap_client_connected(uint8_t max_clients)
{
    uint8_t client_count =
        s_setup_ap_client_count.load(std::memory_order_acquire);
    while (client_count < max_clients &&
           !s_setup_ap_client_count.compare_exchange_weak(
               client_count,
               static_cast<uint8_t>(client_count + 1),
               std::memory_order_acq_rel,
               std::memory_order_acquire)) {
    }
    return s_setup_ap_client_count.load(std::memory_order_acquire);
}

uint8_t wifi_portal_ap_client_disconnected()
{
    uint8_t client_count =
        s_setup_ap_client_count.load(std::memory_order_acquire);
    while (client_count > 0 &&
           !s_setup_ap_client_count.compare_exchange_weak(
               client_count,
               static_cast<uint8_t>(client_count - 1),
               std::memory_order_acq_rel,
               std::memory_order_acquire)) {
    }
    return s_setup_ap_client_count.load(std::memory_order_acquire);
}

bool wifi_portal_should_restart_dhcp()
{
    return s_setup_ap_client_count.load(std::memory_order_acquire) == 0 &&
           setup_portal_active_load() &&
           !s_setup_ap_channel_transition_active.load(
               std::memory_order_acquire) &&
           !wifi_portal_result_preserves_client_lease(
               wifi_portal_save_result_load());
}
