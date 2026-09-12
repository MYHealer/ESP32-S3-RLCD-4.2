// 为小智编解码运行时主机测试声明最小采样率转换 API。
#pragma once

#include <stdint.h>

typedef int esp_ae_err_t;
typedef void *esp_ae_rate_cvt_handle_t;

#define ESP_AE_ERR_OK 0

typedef enum {
    ESP_AE_RATE_CVT_PERF_TYPE_MEMORY = 0,
} esp_ae_rate_cvt_perf_type_t;

typedef struct {
    uint32_t src_rate;
    uint32_t dest_rate;
    uint8_t channel;
    uint8_t bits_per_sample;
    uint8_t complexity;
    esp_ae_rate_cvt_perf_type_t perf_type;
} esp_ae_rate_cvt_cfg_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_ae_err_t esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t *cfg,
                                  esp_ae_rate_cvt_handle_t *handle);
void esp_ae_rate_cvt_close(esp_ae_rate_cvt_handle_t handle);

#ifdef __cplusplus
}
#endif
