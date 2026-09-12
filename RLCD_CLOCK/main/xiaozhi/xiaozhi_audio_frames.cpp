// 实现小智上下行 Opus 编解码、协议封包和播放队列边界保护。
#include "xiaozhi_audio_frames.h"

#include "xiaozhi_protocol_utils.h"
#include "xiaozhi_tts_playback.h"
#include "xiaozhi_voice.h"

#include <arpa/inet.h>
#include <esp_log.h>
#include <esp_opus_dec.h>
#include <esp_opus_enc.h>
#include <esp_transport_ws.h>
#include <freertos/task.h>
#include <string.h>

namespace {

constexpr int kXiaozhiHardwareSampleRate = 16000;
constexpr const char *kTag = "WeatherClock";
constexpr const char *kMalformedAudioFrameLog =
    "Xiaozhi malformed binary audio frame";

} // namespace

bool xiaozhi_decode_incoming_audio(
    xiaozhi_websocket::WebsocketSession *session,
    uint8_t *data,
    size_t len,
    VoiceCodecRuntime *codec_runtime,
    XiaozhiAudioDecodeBuffers *buffers)
{
    if (!session || !data || len == 0 || !codec_runtime ||
        !codec_runtime->decoder || !buffers) {
        return false;
    }
    if (session->discard_tts_audio) {
        return true;
    }
    size_t payload_offset = 0;
    size_t payload_len = 0;
    if (!xiaozhi_protocol::audio_payload_range(session->version,
                                               data,
                                               len,
                                               &payload_offset,
                                               &payload_len)) {
        ESP_LOGW(kTag, "%s", kMalformedAudioFrameLog);
        return false;
    }
    uint8_t *payload = data + payload_offset;
    esp_audio_dec_in_raw_t input = {};
    input.buffer = payload;
    input.len = static_cast<uint32_t>(payload_len);
    esp_audio_dec_out_frame_t output = {};
    output.buffer = reinterpret_cast<uint8_t *>(buffers->decode_pcm);
    output.len = sizeof(buffers->decode_pcm);
    esp_audio_dec_info_t info = {};
    if (esp_opus_dec_decode(codec_runtime->decoder,
                            &input,
                            &output,
                            &info) != ESP_AUDIO_ERR_OK ||
        !xiaozhi_protocol::decoded_audio_size_valid(output.decoded_size,
                                                    sizeof(buffers->decode_pcm))) {
        return false;
    }
    int source_rate = info.sample_rate
                          ? static_cast<int>(info.sample_rate)
                          : session->output_sample_rate;
    const int16_t *playback_samples = buffers->decode_pcm;
    uint32_t playback_sample_count = output.decoded_size / sizeof(int16_t);
    if (source_rate != kXiaozhiHardwareSampleRate) {
        if (!codec_runtime->rate_converter) {
            return false;
        }
        const uint32_t playback_capacity =
            sizeof(buffers->playback_pcm) / sizeof(buffers->playback_pcm[0]);
        uint32_t converted_sample_count = playback_capacity;
        if (esp_ae_rate_cvt_process(codec_runtime->rate_converter,
                                    buffers->decode_pcm,
                                    playback_sample_count,
                                    buffers->playback_pcm,
                                    &converted_sample_count) != ESP_AE_ERR_OK ||
            !xiaozhi_protocol::audio_sample_count_valid(
                converted_sample_count,
                playback_capacity)) {
            return false;
        }
        playback_samples = buffers->playback_pcm;
        playback_sample_count = converted_sample_count;
    }
    if (!session->playback_format_logged) {
        ESP_LOGI(kTag,
                 "TTS audio format: source=%dHz decoded=%u playback=%dHz samples=%u",
                 source_rate,
                 static_cast<unsigned>(output.decoded_size / sizeof(int16_t)),
                 kXiaozhiHardwareSampleRate,
                 static_cast<unsigned>(playback_sample_count));
        session->playback_format_logged = true;
    }
    bool queued = xiaozhi_tts_playback_enqueue(playback_samples,
                                               playback_sample_count);
    if (queued) {
        session->last_tts_audio_tick = xTaskGetTickCount();
        session->turn_assistant_audio_received = true;
        session->empty_reply_continuation_pending = false;
    }
    return queued;
}

bool xiaozhi_send_encoded_microphone(
    xiaozhi_websocket::WebsocketSession *session,
    VoiceCodecRuntime *codec_runtime)
{
    if (!session || !session->socket || !codec_runtime ||
        !codec_runtime->encoder || !codec_runtime->encode_buffers ||
        codec_runtime->encoder_input_size !=
            static_cast<int>(sizeof(codec_runtime->encode_buffers->mono)) ||
        codec_runtime->encoder_output_size <= 0 ||
        codec_runtime->encoder_output_size >
            static_cast<int>(sizeof(codec_runtime->encode_buffers->opus)) ||
        !xiaozhi_voice_read_processed(codec_runtime->encode_buffers->mono,
                                      kXiaozhiOpusFrameSamples,
                                      0)) {
        return false;
    }
    VoiceEncodeBuffers *buffers = codec_runtime->encode_buffers;
    esp_audio_enc_in_frame_t input = {};
    input.buffer = reinterpret_cast<uint8_t *>(buffers->mono);
    input.len = static_cast<uint32_t>(codec_runtime->encoder_input_size);
    esp_audio_enc_out_frame_t output = {};
    output.buffer = buffers->opus;
    output.len = static_cast<uint32_t>(codec_runtime->encoder_output_size);
    if (esp_opus_enc_process(codec_runtime->encoder,
                             &input,
                             &output) != ESP_AUDIO_ERR_OK ||
        output.encoded_bytes == 0) {
        return false;
    }
    if (output.encoded_bytes > sizeof(buffers->opus)) {
        ESP_LOGE(kTag,
                 "Opus packet too large: %u",
                 static_cast<unsigned>(output.encoded_bytes));
        return false;
    }
    const char *payload = reinterpret_cast<const char *>(buffers->opus);
    size_t payload_len = output.encoded_bytes;
    size_t framed_len = 0;
    if (!xiaozhi_protocol::audio_frame_size(session->version,
                                            payload_len,
                                            sizeof(buffers->framed),
                                            &framed_len)) {
        ESP_LOGE(kTag,
                 "Opus frame capacity invalid: version=%d payload=%u capacity=%u",
                 session->version,
                 static_cast<unsigned>(payload_len),
                 static_cast<unsigned>(sizeof(buffers->framed)));
        return false;
    }
    if (session->version == 2) {
        uint16_t protocol_version = htons(static_cast<uint16_t>(session->version));
        uint16_t audio_type = 0;
        uint32_t payload_size = htonl(static_cast<uint32_t>(payload_len));
        memcpy(buffers->framed, &protocol_version, sizeof(protocol_version));
        memcpy(buffers->framed + 2, &audio_type, sizeof(audio_type));
        memset(buffers->framed + 4, 0, 8);
        memcpy(buffers->framed + 12, &payload_size, sizeof(payload_size));
        memcpy(buffers->framed + 16, buffers->opus, payload_len);
        payload = reinterpret_cast<const char *>(buffers->framed);
        payload_len = framed_len;
    } else if (session->version == 3) {
        buffers->framed[0] = 0;
        buffers->framed[1] = 0;
        uint16_t network_len = htons(static_cast<uint16_t>(payload_len));
        memcpy(buffers->framed + 2, &network_len, sizeof(network_len));
        memcpy(buffers->framed + 4, buffers->opus, payload_len);
        payload = reinterpret_cast<const char *>(buffers->framed);
        payload_len = framed_len;
    }
    return esp_transport_ws_send_raw(
               session->socket,
               static_cast<ws_transport_opcodes_t>(
                   WS_TRANSPORT_OPCODES_FIN | WS_TRANSPORT_OPCODES_BINARY),
               payload,
               static_cast<int>(payload_len),
               xiaozhi_websocket::kTimeoutMs) >= 0;
}
