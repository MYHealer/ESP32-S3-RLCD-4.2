// 直接验证设置页反馈和手动同步超时收尾的生产实现。
#include "ui_settings_feedback_internal.h"
#include "app_event_group.h"
#include "app_metadata.h"
#include "app_runtime_timing.h"
#include "network_diagnostics_state.h"
#include "network_sync_request_generation_internal.h"
#include "ui_settings_activity_state_internal.h"

#include <assert.h>
#include <atomic>
#include <stdint.h>
#include <string.h>
#include <thread>

const char *const TAG = "Test";
std::atomic<bool> g_fail_mutex_take{false};

namespace {
TickType_t s_now = 0;
EventBits_t s_event_bits = 0;
int s_notify_count = 0;
int s_network_state_notify_count = 0;
NetworkDiagState s_network_diag_state = kNetworkDiagIdle;

SettingsUiTimingSnapshot sync_timing()
{
    return settings_ui_timing_snapshot_load();
}

void expect_active_feedback(TickType_t now, const char *expected)
{
    SettingsFeedbackSnapshot feedback;
    assert(settings_feedback_snapshot_load(now, &feedback));
    assert(feedback.active);
    assert(strcmp(feedback.text, expected) == 0);
}

void reset_state()
{
    clear_settings_feedback();
    settings_page_request();
    settings_activity_record(0);
    s_network_diag_state = kNetworkDiagIdle;
    assert(!sync_timing().sync_busy);
    s_now = 100;
    s_event_bits = 0;
    s_notify_count = 0;
    s_network_state_notify_count = 0;
}

void expect_timeout(SettingsSyncOp op, EventBits_t bit, const char *feedback)
{
    reset_state();
    constexpr TickType_t kStartTick = 100;
    constexpr TickType_t kDeadlineTick =
        kStartTick + kSettingsManualSyncTimeoutMs;
    s_now = kStartTick;
    begin_settings_sync(op, "同步中", bit);
    assert(!finish_settings_sync_if_timed_out(kDeadlineTick - 1));
    assert(sync_timing().sync_busy);
    assert(sync_timing().sync_deadline_tick == kDeadlineTick);
    assert((s_event_bits & bit) != 0);

    s_now = kDeadlineTick;
    assert(finish_settings_sync_if_timed_out(kDeadlineTick));
    assert(!sync_timing().sync_busy);
    assert(sync_timing().sync_deadline_tick == 0);
    assert((s_event_bits & bit) == 0);
    expect_active_feedback(kDeadlineTick, feedback);
    assert(settings_activity_last_tick() == kDeadlineTick);
    assert(s_notify_count == 2);
    assert(s_network_state_notify_count == 1);
}
} // namespace

NetworkDiagState network_diag_state_load()
{
    return s_network_diag_state;
}

TickType_t xTaskGetTickCount()
{
    return s_now;
}

EventBits_t app_event_group_clear_bits(EventBits_t bits)
{
    EventBits_t previous = s_event_bits;
    s_event_bits &= ~bits;
    return previous;
}

EventBits_t app_event_group_get_bits()
{
    return s_event_bits;
}

EventBits_t app_event_group_set_bits(EventBits_t bits)
{
    s_event_bits |= bits;
    return s_event_bits;
}

void notify_network_sync_runtime_state_changed()
{
    ++s_network_state_notify_count;
}

bool cancel_pending_network_sync_requests(uint32_t request_bits)
{
    const EventBits_t previous_bits =
        app_event_group_clear_bits(request_bits);
    if ((previous_bits & request_bits) == 0) {
        return false;
    }
    notify_network_sync_runtime_state_changed();
    return true;
}

void notify_ui_task()
{
    ++s_notify_count;
}

extern "C" size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t length = strlen(src);
    if (size > 0) {
        size_t copy_length = length < size - 1 ? length : size - 1;
        memcpy(dst, src, copy_length);
        dst[copy_length] = '\0';
    }
    return length;
}

int main()
{
    SettingsFeedbackSnapshot uninitialized_feedback = {};
    uninitialized_feedback.active = true;
    strlcpy(uninitialized_feedback.text,
            "stale",
            sizeof(uninitialized_feedback.text));
    assert(!settings_feedback_snapshot_load(100, &uninitialized_feedback));
    assert(uninitialized_feedback.active);
    assert(strcmp(uninitialized_feedback.text, "stale") == 0);
    assert(init_network_sync_request_generation());
    assert(settings_activity_state_init());
    assert(settings_feedback_state_init());
    assert(settings_feedback_state_init());

    reset_state();
    set_settings_feedback("测试", 2500);
    SettingsUiTimingSnapshot timing = settings_ui_timing_snapshot_load();
    assert(timing.feedback_until_tick == 2600);
    assert(timing.sync_deadline_tick == 0);
    assert(!timing.sync_busy);
    expect_active_feedback(2599, "测试");
    SettingsFeedbackSnapshot expired_feedback;
    assert(settings_feedback_snapshot_load(2600, &expired_feedback));
    assert(!expired_feedback.active);
    assert(expired_feedback.text[0] == '\0');
    assert(settings_activity_last_tick() == 100);
    assert(s_notify_count == 1);

    set_settings_feedback("保留反馈", 2500);
    SettingsFeedbackSnapshot preserved_feedback;
    assert(settings_feedback_snapshot_load(100, &preserved_feedback));
    g_fail_mutex_take.store(true, std::memory_order_release);
    assert(!settings_feedback_snapshot_load(100, &preserved_feedback));
    g_fail_mutex_take.store(false, std::memory_order_release);
    assert(preserved_feedback.active);
    assert(strcmp(preserved_feedback.text, "保留反馈") == 0);

    reset_state();
    begin_settings_sync(kSettingsSyncWeather,
                        "同步中",
                        kManualWeatherSyncBit);
    const SettingsSyncRequestSnapshot weather_request =
        settings_sync_request_snapshot_load();
    timing = settings_ui_timing_snapshot_load();
    assert(timing.feedback_until_tick == 100 + kSettingsManualSyncTimeoutMs);
    assert(timing.sync_deadline_tick == 100 + kSettingsManualSyncTimeoutMs);
    assert(timing.sync_busy);
    expect_active_feedback(100, "同步中");
    finish_settings_sync(kSettingsSyncNtp,
                         weather_request.generation,
                         "错误操作");
    timing = sync_timing();
    assert(timing.sync_busy);
    assert(timing.sync_deadline_tick == 100 + kSettingsManualSyncTimeoutMs);
    finish_settings_sync(kSettingsSyncWeather,
                         weather_request.generation,
                         "同步完成");
    assert(!sync_timing().sync_busy);

    expect_timeout(kSettingsSyncNtp, kManualNtpSyncBit, "时间同步超时");
    expect_timeout(kSettingsSyncWeather, kManualWeatherSyncBit, "天气同步超时");
    expect_timeout(kSettingsSyncSaying, kManualSayingSyncBit, "一言更新超时");

    reset_state();
    s_now = 100;
    begin_settings_sync(kSettingsSyncWeather,
                        "同步中",
                        kManualWeatherSyncBit);
    s_event_bits &= ~kManualWeatherSyncBit;
    const TickType_t settled_request_deadline =
        100 + kSettingsManualSyncTimeoutMs;
    assert(finish_settings_sync_if_timed_out(settled_request_deadline));
    assert(s_network_state_notify_count == 0);

    reset_state();
    s_now = UINT32_MAX - 4 - kSettingsManualSyncTimeoutMs;
    begin_settings_sync(kSettingsSyncNtp,
                        "同步中",
                        kManualNtpSyncBit);
    const TickType_t wrap_deadline = sync_timing().sync_deadline_tick;
    assert(wrap_deadline == UINT32_MAX - 4);
    assert(!finish_settings_sync_if_timed_out(wrap_deadline - 1));
    s_now = 1;
    assert(finish_settings_sync_if_timed_out(wrap_deadline));

    reset_state();
    s_now = 100;
    begin_settings_sync(kSettingsSyncNetworkDiag,
                        "检测中",
                        kNetworkDiagBit);
    const TickType_t network_diag_deadline =
        100 + kSettingsManualSyncTimeoutMs;
    assert(finish_settings_sync_if_timed_out(network_diag_deadline));
    assert(!sync_timing().sync_busy);
    assert(sync_timing().sync_deadline_tick == 0);
    SettingsFeedbackSnapshot feedback;
    assert(settings_feedback_snapshot_load(network_diag_deadline, &feedback));
    assert(!feedback.active);
    assert(feedback.text[0] == '\0');
    assert(s_notify_count == 1);
    assert(s_network_state_notify_count == 0);

    reset_state();
    s_now = 100;
    begin_settings_sync(kSettingsSyncWeather,
                        "旧同步",
                        kManualWeatherSyncBit);
    const SettingsSyncRequestSnapshot old_weather =
        settings_sync_request_snapshot_load();
    s_now = 100 + kSettingsManualSyncTimeoutMs;
    assert(finish_settings_sync_if_timed_out(s_now));
    begin_settings_sync(kSettingsSyncWeather,
                        "新同步",
                        kManualWeatherSyncBit);
    const SettingsSyncRequestSnapshot new_weather =
        settings_sync_request_snapshot_load();
    assert(new_weather.generation != old_weather.generation);
    assert(new_weather.request_generation !=
           old_weather.request_generation);
    finish_settings_sync(kSettingsSyncWeather,
                         old_weather.generation,
                         "旧完成");
    assert(sync_timing().sync_busy);
    assert(settings_sync_request_snapshot_load().generation ==
           new_weather.generation);
    expect_active_feedback(s_now, "新同步");
    finish_settings_sync(kSettingsSyncWeather,
                         new_weather.generation,
                         "新完成");

    reset_state();
    s_now = 100;
    std::atomic<bool> inconsistent{false};
    std::thread writer([] {
        for (int i = 0; i < 10000; ++i) {
            set_settings_feedback((i & 1) == 0 ? "反馈甲" : "反馈乙",
                                  (i & 1) == 0 ? 1000 : 3000);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < 10000; ++i) {
            SettingsFeedbackSnapshot current;
            assert(settings_feedback_snapshot_load(2000, &current));
            if (current.active && strcmp(current.text, "反馈乙") != 0) {
                inconsistent.store(true, std::memory_order_relaxed);
                break;
            }
        }
    });
    writer.join();
    reader.join();
    assert(!inconsistent.load(std::memory_order_relaxed));

    set_settings_feedback("最终反馈", 3000);
    expect_active_feedback(2000, "最终反馈");
    return 0;
}
