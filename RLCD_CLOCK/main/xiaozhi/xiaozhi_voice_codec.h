// 管理小智一轮实时会话的 Opus 编解码器、采样率转换器和借用编码缓冲。
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <esp_ae_rate_cvt.h>

inline constexpr size_t kXiaozhiOpusFrameSamples = 960;
inline constexpr size_t kXiaozhiOpusPacketCapacity = 1280;
inline constexpr size_t kXiaozhiWebsocketAudioHeaderCapacity = 16;

struct VoiceEncodeBuffers {
    int16_t mono[kXiaozhiOpusFrameSamples] = {};
    uint8_t opus[kXiaozhiOpusPacketCapacity] = {};
    uint8_t framed[kXiaozhiOpusPacketCapacity +
                   kXiaozhiWebsocketAudioHeaderCapacity] = {};
};

struct VoiceCodecRuntime {
    void *encoder = nullptr;
    void *decoder = nullptr;
    esp_ae_rate_cvt_handle_t rate_converter = nullptr;
    VoiceEncodeBuffers *encode_buffers = nullptr;
    int encoder_input_size = 0;
    int encoder_output_size = 0;

    VoiceCodecRuntime() = default;
    ~VoiceCodecRuntime();

    VoiceCodecRuntime(const VoiceCodecRuntime &) = delete;
    VoiceCodecRuntime &operator=(const VoiceCodecRuntime &) = delete;

    bool initialize(int output_sample_rate, VoiceEncodeBuffers *borrowed_encode_buffers);
    void release();
};
