// Verifies that WakeNet and realtime uplink remain mutually exclusive.
#include "xiaozhi_voice_pipeline_policy.h"

#include <cassert>

int main()
{
    assert(!xiaozhi_voice_pipeline_uses_wakenet(
        XiaozhiVoicePipelineMode::kNone));
    assert(xiaozhi_voice_pipeline_uses_wakenet(
        XiaozhiVoicePipelineMode::kWakeWord));
    assert(!xiaozhi_voice_pipeline_uses_wakenet(
        XiaozhiVoicePipelineMode::kConversation));

    assert(!xiaozhi_voice_pipeline_streams_uplink(
        XiaozhiVoicePipelineMode::kNone));
    assert(!xiaozhi_voice_pipeline_streams_uplink(
        XiaozhiVoicePipelineMode::kWakeWord));
    assert(xiaozhi_voice_pipeline_streams_uplink(
        XiaozhiVoicePipelineMode::kConversation));
    static_assert(xiaozhi_voice_feed_task_stack_bytes(
                      XiaozhiVoicePipelineMode::kWakeWord) ==
                  kXiaozhiWakeFeedTaskStackBytes);
    static_assert(xiaozhi_voice_feed_task_stack_bytes(
                      XiaozhiVoicePipelineMode::kConversation) ==
                  kXiaozhiConversationFeedTaskStackBytes);
    static_assert(kXiaozhiConversationFeedTaskStackBytes >
                  kXiaozhiWakeFeedTaskStackBytes);
    return 0;
}
