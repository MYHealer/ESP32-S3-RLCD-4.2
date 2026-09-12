// 跟踪公共音频生命周期实际取得的 PM 锁所有权，避免失败申请被误释放。
#pragma once

#include "power_services.h"

class AudioPowerLockOwnership {
public:
    AudioPowerLockOwnership() = default;

    bool acquire()
    {
        if (active_) {
            return true;
        }
        active_ = acquire_audio_awake_lock();
        return active_;
    }

    void release()
    {
        if (!active_) {
            return;
        }
        release_audio_awake_lock();
        active_ = false;
    }

    bool active() const
    {
        return active_;
    }

    AudioPowerLockOwnership(const AudioPowerLockOwnership &) = delete;
    AudioPowerLockOwnership &operator=(const AudioPowerLockOwnership &) = delete;

private:
    bool active_ = false;
};
