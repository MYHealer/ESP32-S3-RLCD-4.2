// Power Demo 仅保留 ES8311 所需的 I2C 和 GPIO 控制接口。
#pragma once

#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t port;
    uint8_t addr;
    void *bus_handle;
} audio_codec_i2c_cfg_t;

const audio_codec_ctrl_if_t *audio_codec_new_i2c_ctrl(
    audio_codec_i2c_cfg_t *i2c_cfg);
const audio_codec_gpio_if_t *audio_codec_new_gpio(void);

#ifdef __cplusplus
}
#endif
