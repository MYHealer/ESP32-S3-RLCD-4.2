// 定义小智唤醒与实时对话互斥使用的 ESP-SR 语音管线。
#pragma once

enum class XiaozhiVoicePipelineMode {
    kNone,
    kWakeWord,
    kConversation,
};

inline constexpr unsigned kXiaozhiWakeFeedTaskStackBytes = 6144;
inline constexpr unsigned kXiaozhiConversationFeedTaskStackBytes = 10240;

constexpr unsigned xiaozhi_voice_feed_task_stack_bytes(
    XiaozhiVoicePipelineMode mode)
{
    return mode == XiaozhiVoicePipelineMode::kConversation
               ? kXiaozhiConversationFeedTaskStackBytes
               : kXiaozhiWakeFeedTaskStackBytes;
}

constexpr bool xiaozhi_voice_pipeline_uses_wakenet(
    XiaozhiVoicePipelineMode mode)
{
    return mode == XiaozhiVoicePipelineMode::kWakeWord;
}

constexpr bool xiaozhi_voice_pipeline_streams_uplink(
    XiaozhiVoicePipelineMode mode)
{
    return mode == XiaozhiVoicePipelineMode::kConversation;
}
