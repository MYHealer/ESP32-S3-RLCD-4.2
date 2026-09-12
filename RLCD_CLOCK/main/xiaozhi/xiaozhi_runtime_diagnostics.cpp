// 汇总小智语音、音频、网络和电源资源状态并输出退出诊断。
#include "xiaozhi_runtime_diagnostics.h"

#include "app_metadata.h"
#include "audio_services.h"
#include "power_services.h"
#include "wifi_radio_state.h"
#include "xiaozhi_tts_playback.h"
#include "xiaozhi_voice.h"

#include <esp_log.h>

namespace {
XiaozhiRuntimeResourceActivity capture_resource_activity(
    const XiaozhiVoiceRuntimeSnapshot &voice,
    const XiaozhiTtsPlaybackSnapshot &playback)
{
    return {
        voice.running,
        voice.feed_task,
        voice.detect_task,
        voice.afe,
        voice.model,
        voice.capture_buffer,
        playback.task_created,
        playback.running,
        playback.busy,
        is_audio_playing(),
        audio_codec_active(),
    };
}
} // namespace

bool xiaozhi_runtime_resources_active(const XiaozhiRuntimeOwnershipSnapshot &ownership)
{
    XiaozhiVoiceRuntimeSnapshot voice = {};
    xiaozhi_voice_get_runtime_snapshot(&voice);
    XiaozhiTtsPlaybackSnapshot playback = {};
    xiaozhi_tts_playback_get_snapshot(&playback);
    return xiaozhi_runtime_resources_active(
        ownership, capture_resource_activity(voice, playback));
}

void xiaozhi_log_shutdown_snapshot()
{
    XiaozhiVoiceRuntimeSnapshot voice = {};
    xiaozhi_voice_get_runtime_snapshot(&voice);
    XiaozhiTtsPlaybackSnapshot playback = {};
    xiaozhi_tts_playback_get_snapshot(&playback);
    PowerLockDepthSnapshot power = {};
    bool power_snapshot_ready = get_power_lock_depth_snapshot(&power);
    ESP_LOGI(TAG,
             "Xiaozhi shutdown: tts_task=%d tts_run=%d tts_busy=%d "
             "voice_run=%d stream=%d feed=%d detect=%d afe=%d model=%d buffer=%d capture=%d "
             "audio=%d codec=%d pm_ok=%d pm_net=%d pm_audio=%d pm_wake=%d pm_cpu=%d wifi=%d",
             playback.task_created ? 1 : 0,
             playback.running ? 1 : 0,
             playback.busy ? 1 : 0,
             voice.running ? 1 : 0,
             voice.streaming ? 1 : 0,
             voice.feed_task ? 1 : 0,
             voice.detect_task ? 1 : 0,
             voice.afe ? 1 : 0,
             voice.model ? 1 : 0,
             voice.processed_stream ? 1 : 0,
             voice.capture_buffer ? 1 : 0,
             is_audio_playing() ? 1 : 0,
             audio_codec_active() ? 1 : 0,
             power_snapshot_ready ? 1 : 0,
             power.network,
             power.audio,
             power.audio_wake,
             power.audio_cpu,
             wifi_radio_on_load() ? 1 : 0);
}
