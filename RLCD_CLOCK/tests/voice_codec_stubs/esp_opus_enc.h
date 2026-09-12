// 为小智编解码运行时主机测试声明最小 Opus encoder API。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int esp_audio_err_t;

#define ESP_AUDIO_ERR_OK 0
#define ESP_AUDIO_SAMPLE_RATE_8K 8000
#define ESP_AUDIO_SAMPLE_RATE_16K 16000
#define ESP_AUDIO_MONO 1
#define ESP_AUDIO_DUAL 2
#define ESP_AUDIO_BIT16 16
#define ESP_OPUS_BITRATE_AUTO (-1000)

typedef enum {
    ESP_OPUS_ENC_FRAME_DURATION_20_MS = 3,
    ESP_OPUS_ENC_FRAME_DURATION_60_MS = 5,
} esp_opus_enc_frame_duration_t;

typedef enum {
    ESP_OPUS_ENC_APPLICATION_VOIP = 0,
    ESP_OPUS_ENC_APPLICATION_AUDIO = 1,
} esp_opus_enc_application_t;

typedef struct {
    int sample_rate;
    int channel;
    int bits_per_sample;
    int bitrate;
    esp_opus_enc_frame_duration_t frame_duration;
    esp_opus_enc_application_t application_mode;
    int complexity;
    bool enable_fec;
    bool enable_dtx;
    bool enable_vbr;
} esp_opus_enc_config_t;

#define ESP_OPUS_ENC_CONFIG_DEFAULT() {                      \
    ESP_AUDIO_SAMPLE_RATE_8K, ESP_AUDIO_DUAL, ESP_AUDIO_BIT16, 90000, \
    ESP_OPUS_ENC_FRAME_DURATION_20_MS, ESP_OPUS_ENC_APPLICATION_VOIP, \
    0, false, false, false                                    \
}

#ifdef __cplusplus
extern "C" {
#endif

esp_audio_err_t esp_opus_enc_open(void *cfg, uint32_t cfg_size, void **handle);
esp_audio_err_t esp_opus_enc_get_frame_size(void *handle, int *input_size, int *output_size);
void esp_opus_enc_close(void *handle);

#ifdef __cplusplus
}
#endif
