// 验证联网请求代次可阻止旧任务清除同类型新请求。
#include "network_sync_request_generation_internal.h"

#include "app_event_group.h"
#include "network_sync_requests.h"

#include <assert.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
EventBits_t s_event_bits = 0;
std::mutex s_event_bits_mutex;
std::mutex s_clear_gate_mutex;
std::condition_variable s_clear_gate_changed;
bool s_block_clear = false;
bool s_clear_entered = false;
bool s_release_clear = false;

EventBits_t current_event_bits()
{
    std::lock_guard<std::mutex> lock(s_event_bits_mutex);
    return s_event_bits;
}
}

EventBits_t app_event_group_get_bits()
{
    return current_event_bits();
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    std::lock_guard<std::mutex> lock(s_event_bits_mutex);
    s_event_bits |= bits;
    return s_event_bits;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    {
        std::unique_lock<std::mutex> lock(s_clear_gate_mutex);
        if (s_block_clear) {
            s_clear_entered = true;
            s_clear_gate_changed.notify_all();
            s_clear_gate_changed.wait(lock, [] {
                return s_release_clear;
            });
        }
    }
    std::lock_guard<std::mutex> lock(s_event_bits_mutex);
    const EventBits_t previous = s_event_bits;
    s_event_bits &= ~bits;
    return previous;
}

int main()
{
    assert(init_network_sync_request_generation());

    NetworkSyncRequestSnapshot scheduled;
    scheduled.manual_weather = true;
    scheduled.manual_weather_generation = 7;
    NetworkSyncRequestSnapshot current = scheduled;
    assert(scheduled.still_owned_by(current));

    current.manual_saying = true;
    current.manual_saying_generation = 3;
    assert(scheduled.still_owned_by(current));

    current.manual_weather_generation = 8;
    assert(!scheduled.still_owned_by(current));
    current = scheduled;
    current.manual_weather = false;
    assert(!scheduled.still_owned_by(current));

    scheduled = {};
    scheduled.provisioning = true;
    scheduled.provisioning_generation = 11;
    current = scheduled;
    current.provisioning_generation = 12;
    assert(!scheduled.still_owned_by(current));

    scheduled = {};
    scheduled.visible_weather = true;
    current = scheduled;
    current.visible_weather = false;
    assert(!scheduled.still_owned_by(current));

    scheduled = {};
    scheduled.diagnostics = true;
    scheduled.diagnostics_generation = 5;
    current = scheduled;
    current.diagnostics_generation = 6;
    assert(!scheduled.still_owned_by(current));

    const uint32_t ntp_first =
        publish_network_sync_request(kManualNtpSyncBit);
    assert(ntp_first != 0);
    assert(network_sync_request_is_current(kManualNtpSyncBit, ntp_first));
    assert(!network_sync_request_is_current(kManualNtpSyncBit,
                                            ntp_first + 1));
    assert((current_event_bits() & kManualNtpSyncBit) != 0);
    assert((current_event_bits() & kNetworkStateChangedBit) != 0);

    NetworkSyncRequestGenerationSnapshot snapshot =
        network_sync_request_generation_snapshot();
    assert(snapshot.manual_ntp == ntp_first);
    assert(snapshot.manual_weather == 0);
    assert(snapshot.manual_saying == 0);
    assert(snapshot.diagnostics == 0);

    assert(retire_network_sync_request(kManualNtpSyncBit, ntp_first));
    assert(!network_sync_request_is_current(kManualNtpSyncBit, ntp_first));
    assert((current_event_bits() & kManualNtpSyncBit) == 0);
    assert(!retire_network_sync_request(kManualNtpSyncBit, ntp_first));

    const uint32_t weather_first =
        publish_network_sync_request(kManualWeatherSyncBit);
    const uint32_t weather_second =
        publish_network_sync_request(kManualWeatherSyncBit);
    assert(weather_second != 0);
    assert(weather_second != weather_first);
    assert(!network_sync_request_is_current(kManualWeatherSyncBit,
                                            weather_first));
    assert(network_sync_request_is_current(kManualWeatherSyncBit,
                                           weather_second));
    assert(!retire_network_sync_request(kManualWeatherSyncBit,
                                        weather_first));
    assert((current_event_bits() & kManualWeatherSyncBit) != 0);
    assert(retire_network_sync_request(kManualWeatherSyncBit,
                                       weather_second));
    assert((current_event_bits() & kManualWeatherSyncBit) == 0);

    const uint32_t saying =
        publish_network_sync_request(kManualSayingSyncBit);
    const uint32_t diagnostics =
        publish_network_sync_request(kNetworkDiagBit);
    invalidate_network_sync_requests(kManualSayingSyncBit |
                                     kNetworkDiagBit);
    assert((current_event_bits() &
            (kManualSayingSyncBit | kNetworkDiagBit)) == 0);
    assert(!retire_network_sync_request(kManualSayingSyncBit, saying));
    assert(!retire_network_sync_request(kNetworkDiagBit, diagnostics));
    assert(!network_sync_request_is_current(kManualSayingSyncBit, saying));
    assert(!network_sync_request_is_current(kNetworkDiagBit, diagnostics));

    const uint32_t weather_before_race =
        publish_network_sync_request(kManualWeatherSyncBit);
    {
        std::lock_guard<std::mutex> lock(s_clear_gate_mutex);
        s_block_clear = true;
        s_clear_entered = false;
        s_release_clear = false;
    }
    std::thread invalidator([] {
        invalidate_network_sync_requests(kManualWeatherSyncBit);
    });
    {
        std::unique_lock<std::mutex> lock(s_clear_gate_mutex);
        s_clear_gate_changed.wait(lock, [] {
            return s_clear_entered;
        });
    }

    std::atomic<bool> publisher_done{false};
    uint32_t weather_during_race = 0;
    std::thread publisher([&] {
        weather_during_race =
            publish_network_sync_request(kManualWeatherSyncBit);
        publisher_done.store(true, std::memory_order_release);
    });
    const auto publisher_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (!publisher_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < publisher_deadline) {
        std::this_thread::yield();
    }
    {
        std::lock_guard<std::mutex> lock(s_clear_gate_mutex);
        s_release_clear = true;
    }
    s_clear_gate_changed.notify_all();
    invalidator.join();
    publisher.join();
    {
        std::lock_guard<std::mutex> lock(s_clear_gate_mutex);
        s_block_clear = false;
    }

    assert(weather_during_race != 0);
    assert(weather_during_race != weather_before_race);
    assert((current_event_bits() & kManualWeatherSyncBit) != 0);
    assert(retire_network_sync_request(kManualWeatherSyncBit,
                                       weather_during_race));

    assert(publish_network_sync_request(
               kManualNtpSyncBit | kManualWeatherSyncBit) == 0);
    assert(!network_sync_request_is_current(
        kManualNtpSyncBit | kManualWeatherSyncBit, 1));
    assert(!network_sync_request_is_current(kManualNtpSyncBit, 0));
    assert(!retire_network_sync_request(1U << 31, 1));
    return 0;
}
