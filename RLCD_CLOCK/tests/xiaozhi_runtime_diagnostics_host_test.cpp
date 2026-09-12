// 验证小智运行资源活动判断覆盖全部所有权和底层资源字段。
#include "xiaozhi_runtime_diagnostics.h"

constexpr XiaozhiRuntimeOwnershipSnapshot kNoOwnership = {};
constexpr XiaozhiRuntimeResourceActivity kNoActivity = {};

constexpr bool every_resource_activity_is_detected()
{
    for (int index = 0; index < 11; ++index) {
        XiaozhiRuntimeResourceActivity activity = {};
        switch (index) {
        case 0:
            activity.voice_running = true;
            break;
        case 1:
            activity.voice_feed_task = true;
            break;
        case 2:
            activity.voice_detect_task = true;
            break;
        case 3:
            activity.voice_afe = true;
            break;
        case 4:
            activity.voice_model = true;
            break;
        case 5:
            activity.voice_capture_buffer = true;
            break;
        case 6:
            activity.tts_task_created = true;
            break;
        case 7:
            activity.tts_running = true;
            break;
        case 8:
            activity.tts_busy = true;
            break;
        case 9:
            activity.audio_playing = true;
            break;
        case 10:
            activity.codec_active = true;
            break;
        default:
            return false;
        }
        if (!xiaozhi_runtime_resources_active(kNoOwnership, activity)) {
            return false;
        }
    }
    return true;
}

static_assert(!xiaozhi_runtime_resources_active(kNoOwnership, kNoActivity));
static_assert(xiaozhi_runtime_resources_active({true, false, false, false}, kNoActivity));
static_assert(xiaozhi_runtime_resources_active({false, true, false, false}, kNoActivity));
static_assert(xiaozhi_runtime_resources_active({false, false, true, false}, kNoActivity));
static_assert(xiaozhi_runtime_resources_active({false, false, false, true}, kNoActivity));
static_assert(every_resource_activity_is_detected());

int main()
{
    return 0;
}
