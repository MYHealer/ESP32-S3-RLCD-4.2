// 定义小智运行资源活动判断和退出诊断入口。
#pragma once

struct XiaozhiRuntimeOwnershipSnapshot {
    bool voice_started;
    bool network_keepalive;
    bool network_lock_held;
    bool idle_low_power;
};

struct XiaozhiRuntimeResourceActivity {
    bool voice_running;
    bool voice_feed_task;
    bool voice_detect_task;
    bool voice_afe;
    bool voice_model;
    bool voice_capture_buffer;
    bool tts_task_created;
    bool tts_running;
    bool tts_busy;
    bool audio_playing;
    bool codec_active;
};

constexpr bool xiaozhi_runtime_resources_active(
    const XiaozhiRuntimeOwnershipSnapshot &ownership,
    const XiaozhiRuntimeResourceActivity &activity)
{
    return ownership.voice_started || ownership.network_keepalive ||
           ownership.network_lock_held || ownership.idle_low_power ||
           activity.voice_running || activity.voice_feed_task ||
           activity.voice_detect_task || activity.voice_afe ||
           activity.voice_model || activity.voice_capture_buffer ||
           activity.tts_task_created ||
           activity.tts_running || activity.tts_busy ||
           activity.audio_playing || activity.codec_active;
}

bool xiaozhi_runtime_resources_active(const XiaozhiRuntimeOwnershipSnapshot &ownership);
void xiaozhi_log_shutdown_snapshot();
