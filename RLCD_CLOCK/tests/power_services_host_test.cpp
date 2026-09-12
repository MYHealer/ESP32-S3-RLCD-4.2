// 验证网络与音频 PM 锁的嵌套计数、失败回滚和作用域互斥释放语义。
#include "power_services_internal.h"

#include "app_metadata.h"

#include <esp_err.h>
#include <esp_pm.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

const char *const TAG = "PowerServicesHostTest";
bool g_fail_mutex_create = false;
bool g_fail_mutex_take = false;
int g_mutex_create_calls = 0;
uint32_t g_last_mutex_take_timeout = 0;
int g_unbounded_mutex_take_calls = 0;

struct FakePmLock {
    const char *name = nullptr;
    int acquire_attempts = 0;
    int release_attempts = 0;
    bool held = false;
};

namespace {
FakePmLock s_locks[4] = {};
int s_lock_count = 0;
int s_lock_create_attempts = 0;
int s_configure_calls = 0;
bool s_fail_configure = false;
const char *s_fail_create_name = nullptr;
const char *s_fail_acquire_name = nullptr;
const char *s_fail_release_name = nullptr;

FakePmLock *find_lock(const char *name)
{
    for (int i = 0; i < s_lock_count; ++i) {
        if (strcmp(s_locks[i].name, name) == 0) {
            return &s_locks[i];
        }
    }
    return nullptr;
}

void expect_depths(int network, int audio, int wake, int cpu)
{
    PowerLockDepthSnapshot snapshot = {};
    assert(get_power_lock_depth_snapshot(&snapshot));
    assert(snapshot.network == network);
    assert(snapshot.audio == audio);
    assert(snapshot.audio_wake == wake);
    assert(snapshot.audio_cpu == cpu);
}
} // namespace

esp_err_t esp_pm_configure(const esp_pm_config_t *)
{
    ++s_configure_calls;
    return s_fail_configure ? ESP_FAIL : ESP_OK;
}

esp_err_t esp_pm_lock_create(esp_pm_lock_type_t,
                             int,
                             const char *name,
                             esp_pm_lock_handle_t *out)
{
    assert(out);
    ++s_lock_create_attempts;
    if (s_fail_create_name && strcmp(name, s_fail_create_name) == 0) {
        *out = nullptr;
        return ESP_FAIL;
    }
    assert(s_lock_count < static_cast<int>(sizeof(s_locks) / sizeof(s_locks[0])));
    FakePmLock *lock = &s_locks[s_lock_count++];
    lock->name = name;
    *out = lock;
    return ESP_OK;
}

esp_err_t esp_pm_lock_acquire(esp_pm_lock_handle_t lock)
{
    assert(lock);
    ++lock->acquire_attempts;
    if (s_fail_acquire_name && strcmp(lock->name, s_fail_acquire_name) == 0) {
        return ESP_FAIL;
    }
    assert(!lock->held);
    lock->held = true;
    return ESP_OK;
}

esp_err_t esp_pm_lock_release(esp_pm_lock_handle_t lock)
{
    assert(lock);
    ++lock->release_attempts;
    if (s_fail_release_name && strcmp(lock->name, s_fail_release_name) == 0) {
        return ESP_FAIL;
    }
    if (!lock->held) {
        return ESP_ERR_INVALID_STATE;
    }
    lock->held = false;
    return ESP_OK;
}

const char *esp_err_to_name(esp_err_t err)
{
    if (err == ESP_OK) {
        return "ESP_OK";
    }
    return err == ESP_ERR_INVALID_STATE ? "ESP_ERR_INVALID_STATE" : "ESP_FAIL";
}

int main()
{
    PowerLockDepthSnapshot unavailable = {};
    assert(network_awake_lock_active());
    assert(!get_power_lock_depth_snapshot(&unavailable));

    g_fail_mutex_create = true;
    init_power_management();
    assert(g_mutex_create_calls == 1);
    assert(s_configure_calls == 0);
    assert(s_lock_count == 0);
    assert(network_awake_lock_active());
    assert(!get_power_lock_depth_snapshot(&unavailable));

    g_fail_mutex_create = false;
    s_fail_configure = true;
    s_fail_create_name = "audio_wake_80";
    init_power_management();
    assert(s_configure_calls == 1);
    assert(s_lock_create_attempts == 4);
    assert(s_lock_count == 3);
    assert(find_lock("network_sync"));
    assert(find_lock("audio_play"));
    assert(!find_lock("audio_wake_80"));
    assert(find_lock("audio_cpu_max"));

    s_fail_configure = false;
    s_fail_create_name = nullptr;
    init_power_management();
    assert(g_mutex_create_calls == 2);
    assert(s_configure_calls == 2);
    assert(s_lock_create_attempts == 5);
    assert(s_lock_count == 4);

    init_power_management();
    assert(s_configure_calls == 2);
    assert(s_lock_create_attempts == 5);
    assert(s_lock_count == 4);
    expect_depths(0, 0, 0, 0);

    FakePmLock *network = find_lock("network_sync");
    FakePmLock *audio = find_lock("audio_play");
    FakePmLock *wake = find_lock("audio_wake_80");
    FakePmLock *cpu = find_lock("audio_cpu_max");
    assert(network && audio && wake && cpu);

    assert(acquire_network_awake_lock());
    assert(g_last_mutex_take_timeout == 1000);
    assert(acquire_network_awake_lock());
    assert(g_last_mutex_take_timeout == 1000);
    assert(network->acquire_attempts == 1);
    expect_depths(2, 0, 0, 0);
    release_network_awake_lock();
    assert(g_last_mutex_take_timeout == UINT32_MAX);
    assert(network->release_attempts == 0);
    expect_depths(1, 0, 0, 0);
    release_network_awake_lock();
    assert(network->release_attempts == 1);
    expect_depths(0, 0, 0, 0);
    release_network_awake_lock();
    assert(network->release_attempts == 1);

    g_fail_mutex_take = true;
    assert(!acquire_network_awake_lock());
    assert(network_awake_lock_active());
    assert(!get_power_lock_depth_snapshot(&unavailable));
    g_fail_mutex_take = false;
    expect_depths(0, 0, 0, 0);

    assert(acquire_network_awake_lock());
    s_fail_release_name = "network_sync";
    release_network_awake_lock();
    expect_depths(1, 0, 0, 0);
    s_fail_release_name = nullptr;
    release_network_awake_lock();
    expect_depths(0, 0, 0, 0);

    assert(acquire_network_awake_lock());
    const int network_acquires_before_stale_driver_state =
        network->acquire_attempts;
    network->held = false;
    release_network_awake_lock();
    expect_depths(0, 0, 0, 0);
    assert(acquire_network_awake_lock());
    assert(network->acquire_attempts ==
           network_acquires_before_stale_driver_state + 1);
    release_network_awake_lock();
    expect_depths(0, 0, 0, 0);

    assert(acquire_audio_awake_lock());
    expect_depths(0, 1, 1, 1);
    const int cpu_acquires = cpu->acquire_attempts;
    assert(set_audio_performance_mode(true));
    assert(cpu->acquire_attempts == cpu_acquires);
    const int unbounded_takes_before_audio_release =
        g_unbounded_mutex_take_calls;
    release_audio_awake_lock();
    assert(g_unbounded_mutex_take_calls ==
           unbounded_takes_before_audio_release + 3);
    expect_depths(0, 0, 0, 0);

    const int audio_releases_before_wake_failure = audio->release_attempts;
    s_fail_acquire_name = "audio_wake_80";
    assert(!acquire_audio_awake_lock());
    s_fail_acquire_name = nullptr;
    assert(audio->release_attempts == audio_releases_before_wake_failure + 1);
    expect_depths(0, 0, 0, 0);

    const int audio_releases_before_cpu_failure = audio->release_attempts;
    const int wake_releases_before_cpu_failure = wake->release_attempts;
    s_fail_acquire_name = "audio_cpu_max";
    assert(!acquire_audio_awake_lock());
    s_fail_acquire_name = nullptr;
    assert(audio->release_attempts == audio_releases_before_cpu_failure + 1);
    assert(wake->release_attempts == wake_releases_before_cpu_failure + 1);
    expect_depths(0, 0, 0, 0);

    assert(acquire_audio_awake_lock());
    const int cpu_acquires_before_stale_driver_state = cpu->acquire_attempts;
    cpu->held = false;
    release_audio_awake_lock();
    expect_depths(0, 0, 0, 0);
    assert(acquire_audio_awake_lock());
    assert(cpu->acquire_attempts ==
           cpu_acquires_before_stale_driver_state + 1);
    release_audio_awake_lock();
    expect_depths(0, 0, 0, 0);

    assert(acquire_audio_awake_lock());
    s_fail_release_name = "audio_cpu_max";
    assert(!set_audio_performance_mode(false));
    expect_depths(0, 1, 1, 1);
    s_fail_release_name = nullptr;
    assert(set_audio_performance_mode(false));
    expect_depths(0, 1, 1, 0);
    assert(set_audio_performance_mode(false));
    release_audio_awake_lock();
    expect_depths(0, 0, 0, 0);
    return 0;
}
