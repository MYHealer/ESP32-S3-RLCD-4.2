// 维护麦克风连续读取失败计数和有界恢复判定，避免采集任务无限空转。
#pragma once

#include <cstdint>

struct XiaozhiVoiceReadHealthResult {
    uint32_t consecutive_failures = 0;
    bool should_log = false;
    bool should_rebuild = false;
};

inline XiaozhiVoiceReadHealthResult xiaozhi_voice_read_health_after_result(
    uint32_t consecutive_failures,
    bool read_ok,
    uint32_t failure_limit)
{
    if (read_ok) {
        return {};
    }
    uint32_t next_failures = consecutive_failures;
    if (next_failures < UINT32_MAX) {
        ++next_failures;
    }
    bool limit_reached = failure_limit == 0 || next_failures >= failure_limit;
    return {
        next_failures,
        next_failures == 1 || limit_reached,
        limit_reached,
    };
}
