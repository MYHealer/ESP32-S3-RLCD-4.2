// 为小智编解码运行时主机测试声明最小 Opus decoder API。
#pragma once

#include "esp_opus_enc.h"

typedef enum {
    ESP_OPUS_DEC_FRAME_DURATION_INVALID = -1,
    ESP_OPUS_DEC_FRAME_DURATION_60_MS = 5,
} esp_opus_dec_frame_duration_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t channel;
    esp_opus_dec_frame_duration_t frame_duration;
    bool self_delimited;
} esp_opus_dec_cfg_t;

#define ESP_OPUS_DEC_CONFIG_DEFAULT() { \
    ESP_AUDIO_SAMPLE_RATE_8K, ESP_AUDIO_DUAL, ESP_OPUS_DEC_FRAME_DURATION_INVALID, false \
}

#ifdef __cplusplus
extern "C" {
#endif

esp_audio_err_t esp_opus_dec_open(void *cfg, uint32_t cfg_size, void **handle);
esp_audio_err_t esp_opus_dec_close(void *handle);

#ifdef __cplusplus
}
#endif
