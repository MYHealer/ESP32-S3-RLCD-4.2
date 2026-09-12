// 验证音频 PM 锁只在实际取得后发布所有权并执行一次释放。
#include "audio_power_lock_ownership.h"

#include <assert.h>

namespace {
bool g_lock_available = true;
int g_acquire_calls = 0;
int g_release_calls = 0;
} // namespace

bool acquire_audio_awake_lock()
{
    ++g_acquire_calls;
    return g_lock_available;
}

void release_audio_awake_lock()
{
    ++g_release_calls;
}

int main()
{
    AudioPowerLockOwnership ownership;
    assert(!ownership.active());

    g_lock_available = false;
    assert(!ownership.acquire());
    assert(!ownership.active());
    ownership.release();
    assert(g_acquire_calls == 1);
    assert(g_release_calls == 0);

    g_lock_available = true;
    assert(ownership.acquire());
    assert(ownership.acquire());
    assert(ownership.active());
    assert(g_acquire_calls == 2);

    ownership.release();
    ownership.release();
    assert(!ownership.active());
    assert(g_release_calls == 1);
    return 0;
}
