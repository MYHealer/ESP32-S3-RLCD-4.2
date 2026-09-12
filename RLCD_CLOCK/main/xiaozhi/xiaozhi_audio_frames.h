// 声明小智上下行 Opus/WebSocket 音频帧处理及解码临时缓冲。
#pragma once

#include "xiaozhi_voice_codec.h"
#include "xiaozhi_websocket_session.h"

#include <cstddef>
#include <cstdint>

struct XiaozhiAudioDecodeBuffers {
    int16_t decode_pcm[2880] = {};
    int16_t playback_pcm[1600] = {};
};

bool xiaozhi_decode_incoming_audio(
    xiaozhi_websocket::WebsocketSession *session,
    uint8_t *data,
    size_t len,
    VoiceCodecRuntime *codec_runtime,
    XiaozhiAudioDecodeBuffers *buffers);

bool xiaozhi_send_encoded_microphone(
    xiaozhi_websocket::WebsocketSession *session,
    VoiceCodecRuntime *codec_runtime);
