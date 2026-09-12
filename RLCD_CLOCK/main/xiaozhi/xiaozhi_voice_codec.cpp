// 实现小智实时语音编解码资源的固定参数初始化与幂等释放。
#include "xiaozhi_voice_codec.h"

#include "app_metadata.h"

#include <esp_log.h>
#include <esp_opus_dec.h>
#include <esp_opus_enc.h>

#include <cstring>

namespace {
constexpr int kXiaozhiHardwareSampleRate = 16000;
}

VoiceCodecRuntime::~VoiceCodecRuntime()
{
    release();
}

bool VoiceCodecRuntime::initialize(int output_sample_rate,
                                   VoiceEncodeBuffers *borrowed_encode_buffers)
{
    release();

    esp_opus_enc_config_t encoder_cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    encoder_cfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K;
    encoder_cfg.channel = ESP_AUDIO_MONO;
    encoder_cfg.bits_per_sample = ESP_AUDIO_BIT16;
    encoder_cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS;
    encoder_cfg.bitrate = ESP_OPUS_BITRATE_AUTO;
    encoder_cfg.application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO;
    encoder_cfg.enable_dtx = true;
    encoder_cfg.enable_vbr = true;

    esp_opus_dec_cfg_t decoder_cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
    decoder_cfg.sample_rate = output_sample_rate;
    decoder_cfg.channel = ESP_AUDIO_MONO;
    decoder_cfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS;
    bool ready = esp_opus_enc_open(&encoder_cfg,
                                   sizeof(encoder_cfg),
                                   &encoder) == ESP_AUDIO_ERR_OK &&
                 esp_opus_dec_open(&decoder_cfg,
                                   sizeof(decoder_cfg),
                                   &decoder) == ESP_AUDIO_ERR_OK;
    if (ready && output_sample_rate != kXiaozhiHardwareSampleRate) {
        esp_ae_rate_cvt_cfg_t rate_cfg = {};
        rate_cfg.src_rate = static_cast<uint32_t>(output_sample_rate);
        rate_cfg.dest_rate = kXiaozhiHardwareSampleRate;
        rate_cfg.channel = 1;
        rate_cfg.bits_per_sample = 16;
        rate_cfg.complexity = 2;
        rate_cfg.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_MEMORY;
        ready = esp_ae_rate_cvt_open(&rate_cfg, &rate_converter) == ESP_AE_ERR_OK &&
                rate_converter != nullptr;
    }
    if (ready &&
        esp_opus_enc_get_frame_size(encoder,
                                    &encoder_input_size,
                                    &encoder_output_size) != ESP_AUDIO_ERR_OK) {
        ready = false;
    }
    if (ready &&
        (encoder_input_size != static_cast<int>(sizeof(VoiceEncodeBuffers::mono)) ||
         encoder_output_size <= 0 ||
         encoder_output_size > static_cast<int>(sizeof(VoiceEncodeBuffers::opus)))) {
        ESP_LOGE(TAG,
                 "Unsupported Opus frame sizes: input=%d expected=%u output=%d capacity=%u",
                 encoder_input_size,
                 static_cast<unsigned>(sizeof(VoiceEncodeBuffers::mono)),
                 encoder_output_size,
                 static_cast<unsigned>(sizeof(VoiceEncodeBuffers::opus)));
        ready = false;
    }
    if (ready && borrowed_encode_buffers) {
        std::memset(borrowed_encode_buffers, 0, sizeof(VoiceEncodeBuffers));
        encode_buffers = borrowed_encode_buffers;
    } else if (ready) {
        ready = false;
    }
    if (!ready) {
        release();
    }
    return ready;
}

void VoiceCodecRuntime::release()
{
    if (encoder) {
        esp_opus_enc_close(encoder);
        encoder = nullptr;
    }
    if (decoder) {
        esp_opus_dec_close(decoder);
        decoder = nullptr;
    }
    if (rate_converter) {
        esp_ae_rate_cvt_close(rate_converter);
        rate_converter = nullptr;
    }
    encode_buffers = nullptr;
    encoder_input_size = 0;
    encoder_output_size = 0;
}
