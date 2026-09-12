// 验证麦克风读取瞬时失败容忍、成功复位和持续失败重建边界。
#include "xiaozhi_voice_read_health.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    constexpr uint32_t kLimit = 3;

    XiaozhiVoiceReadHealthResult result =
        xiaozhi_voice_read_health_after_result(0, false, kLimit);
    assert(result.consecutive_failures == 1);
    assert(result.should_log);
    assert(!result.should_rebuild);

    result = xiaozhi_voice_read_health_after_result(1, false, kLimit);
    assert(result.consecutive_failures == 2);
    assert(!result.should_log);
    assert(!result.should_rebuild);

    result = xiaozhi_voice_read_health_after_result(2, false, kLimit);
    assert(result.consecutive_failures == 3);
    assert(result.should_log);
    assert(result.should_rebuild);

    result = xiaozhi_voice_read_health_after_result(2, true, kLimit);
    assert(result.consecutive_failures == 0);
    assert(!result.should_log);
    assert(!result.should_rebuild);

    result = xiaozhi_voice_read_health_after_result(UINT32_MAX, false, kLimit);
    assert(result.consecutive_failures == UINT32_MAX);
    assert(result.should_rebuild);

    result = xiaozhi_voice_read_health_after_result(0, false, 0);
    assert(result.should_log);
    assert(result.should_rebuild);
    return 0;
}
