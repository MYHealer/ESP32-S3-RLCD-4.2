// 验证小智实时/待唤醒电源状态切换、同状态校准和失败回滚。
#include "xiaozhi_power_session.h"

#include "app_event_group.h"
#include "audio_services.h"
#include "power_services.h"
#include "wifi_radio_services.h"
#include "wifi_radio_state.h"

#include <esp_err.h>
#include <esp_wifi.h>
#include <freertos/task.h>

#include <assert.h>

extern const char *const TAG = "XiaozhiPowerSessionHostTest";

namespace {
int s_network_lock_depth = 0;
bool s_fail_network_lock_acquire = false;
bool s_wifi_radio_on = true;
int s_wifi_fail_mode = -1;
int s_wifi_fail_remaining = 0;
int s_wifi_set_calls[3] = {};
int s_delay_calls = 0;
bool s_fail_audio_high_performance = false;
bool s_fail_audio_low_performance = false;
int s_audio_high_performance_calls = 0;
int s_audio_low_performance_calls = 0;
int s_wifi_stop_requests = 0;

void fail_wifi_mode(wifi_ps_type_t mode)
{
    s_wifi_fail_mode = static_cast<int>(mode);
    s_wifi_fail_remaining = 3;
}

void clear_wifi_failure()
{
    s_wifi_fail_mode = -1;
    s_wifi_fail_remaining = 0;
}

void expect_power_state(bool keepalive, bool lock_held, bool idle)
{
    const XiaozhiPowerSessionSnapshot snapshot = xiaozhi_power_session_snapshot();
    assert(snapshot.network_keepalive == keepalive);
    assert(snapshot.network_lock_held == lock_held);
    assert(snapshot.idle_low_power == idle);
}
} // namespace

bool app_event_group_ready()
{
    return true;
}

EventBits_t app_event_group_get_bits()
{
    return kWifiConnectedBit;
}

bool acquire_network_awake_lock()
{
    if (s_fail_network_lock_acquire) {
        return false;
    }
    ++s_network_lock_depth;
    return true;
}

void release_network_awake_lock()
{
    assert(s_network_lock_depth > 0);
    --s_network_lock_depth;
}

bool network_awake_lock_active()
{
    return s_network_lock_depth > 0;
}

bool wifi_always_on_required()
{
    return false;  // host 测试不模拟 MiPlay/airkan 常驻策略
}

bool set_xiaozhi_audio_high_performance(bool enabled)
{
    if (enabled) {
        ++s_audio_high_performance_calls;
        return !s_fail_audio_high_performance;
    }
    ++s_audio_low_performance_calls;
    return !s_fail_audio_low_performance;
}

bool start_wifi_radio(bool)
{
    s_wifi_radio_on = true;
    return true;
}

bool wait_for_wifi_connected(uint32_t, EventBits_t)
{
    return true;
}

void request_wifi_radio_stop_when_idle()
{
    ++s_wifi_stop_requests;
}

bool wifi_radio_on_load()
{
    return s_wifi_radio_on;
}

esp_err_t esp_wifi_set_ps(wifi_ps_type_t mode)
{
    ++s_wifi_set_calls[static_cast<int>(mode)];
    if (s_wifi_fail_remaining > 0 && s_wifi_fail_mode == static_cast<int>(mode)) {
        --s_wifi_fail_remaining;
        return ESP_FAIL;
    }
    return ESP_OK;
}

const char *esp_err_to_name(esp_err_t err)
{
    return err == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

void vTaskDelay(TickType_t)
{
    ++s_delay_calls;
}

int main()
{
    expect_power_state(false, false, false);

    // Initial realtime entry must not publish keepalive when the Wi-Fi driver
    // cannot leave modem sleep after all bounded retries.
    fail_wifi_mode(WIFI_PS_NONE);
    assert(!xiaozhi_power_session_acquire_realtime());
    expect_power_state(false, true, false);
    assert(s_network_lock_depth == 1);
    assert(s_wifi_set_calls[WIFI_PS_NONE] == 3);
    assert(s_delay_calls == 2);
    clear_wifi_failure();
    xiaozhi_power_session_release();
    expect_power_state(false, false, false);
    assert(s_network_lock_depth == 0);

    assert(xiaozhi_power_session_acquire_realtime());
    expect_power_state(true, true, false);
    assert(s_network_lock_depth == 1);

    assert(xiaozhi_power_session_set_idle(true));
    expect_power_state(true, false, true);
    assert(s_network_lock_depth == 0);
    assert(s_audio_low_performance_calls == 1);

    // WakeNet rebuilds its audio session while logical state remains idle.
    // The repeated request must still release the newly acquired CPU MAX lock.
    assert(xiaozhi_power_session_set_idle(true));
    assert(s_audio_low_performance_calls == 2);

    assert(xiaozhi_power_session_set_idle(false));
    expect_power_state(true, true, false);
    assert(s_network_lock_depth == 1);
    assert(s_audio_high_performance_calls == 1);

    const int low_calls_before_wifi_failure = s_audio_low_performance_calls;
    const int delays_before_idle_wifi_failure = s_delay_calls;
    fail_wifi_mode(WIFI_PS_MAX_MODEM);
    assert(!xiaozhi_power_session_set_idle(true));
    expect_power_state(true, true, false);
    assert(s_network_lock_depth == 1);
    assert(s_audio_low_performance_calls == low_calls_before_wifi_failure);
    assert(s_delay_calls == delays_before_idle_wifi_failure + 2);
    clear_wifi_failure();

    s_fail_audio_low_performance = true;
    const int none_calls_before_idle_rollback = s_wifi_set_calls[WIFI_PS_NONE];
    assert(!xiaozhi_power_session_set_idle(true));
    expect_power_state(true, true, false);
    assert(s_network_lock_depth == 1);
    assert(s_wifi_set_calls[WIFI_PS_NONE] == none_calls_before_idle_rollback + 1);
    s_fail_audio_low_performance = false;

    assert(xiaozhi_power_session_set_idle(true));
    expect_power_state(true, false, true);
    assert(s_network_lock_depth == 0);

    const int delays_before_realtime_wifi_failure = s_delay_calls;
    fail_wifi_mode(WIFI_PS_NONE);
    assert(!xiaozhi_power_session_set_idle(false));
    expect_power_state(true, false, true);
    assert(s_network_lock_depth == 0);
    assert(s_delay_calls == delays_before_realtime_wifi_failure + 2);
    clear_wifi_failure();

    s_fail_audio_high_performance = true;
    const int max_calls_before_realtime_rollback = s_wifi_set_calls[WIFI_PS_MAX_MODEM];
    assert(!xiaozhi_power_session_set_idle(false));
    expect_power_state(true, false, true);
    assert(s_network_lock_depth == 0);
    assert(s_wifi_set_calls[WIFI_PS_MAX_MODEM] == max_calls_before_realtime_rollback + 1);
    s_fail_audio_high_performance = false;

    assert(xiaozhi_power_session_set_idle(false));
    expect_power_state(true, true, false);
    assert(s_network_lock_depth == 1);

    // Repeated realtime requests also reconcile a newly rebuilt audio owner.
    const int high_calls_before_reconcile = s_audio_high_performance_calls;
    assert(xiaozhi_power_session_set_idle(false));
    assert(s_audio_high_performance_calls == high_calls_before_reconcile + 1);

    const int max_calls_before_release = s_wifi_set_calls[WIFI_PS_MAX_MODEM];
    const int min_calls_before_release = s_wifi_set_calls[WIFI_PS_MIN_MODEM];
    xiaozhi_power_session_release();
    expect_power_state(false, false, false);
    assert(s_network_lock_depth == 0);
    assert(s_wifi_stop_requests == 2);
    assert(s_wifi_set_calls[WIFI_PS_MAX_MODEM] == max_calls_before_release + 1);
    assert(s_wifi_set_calls[WIFI_PS_MIN_MODEM] == min_calls_before_release);

    // Page exit uses the same bounded retry policy as the other Xiaozhi power
    // transitions. Even a persistent driver error must not retain logical
    // keepalive or the network lock after the radio stop has been requested.
    assert(xiaozhi_power_session_acquire_realtime());
    const int delays_before_release_failure = s_delay_calls;
    fail_wifi_mode(WIFI_PS_MAX_MODEM);
    xiaozhi_power_session_release();
    expect_power_state(false, false, false);
    assert(s_network_lock_depth == 0);
    assert(s_wifi_set_calls[WIFI_PS_MAX_MODEM] == max_calls_before_release + 4);
    assert(s_delay_calls == delays_before_release_failure + 2);
    assert(s_wifi_stop_requests == 3);
    clear_wifi_failure();
    return 0;
}
