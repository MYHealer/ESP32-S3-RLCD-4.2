// 只保留 Demo 实际使用的三个释放入口。
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "audio_codec_if.h"

#include <stdlib.h>

int audio_codec_delete_codec_if(const audio_codec_if_t *codec)
{
    if (!codec) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int result = codec->close ? codec->close(codec) : ESP_CODEC_DEV_OK;
    free((void *)codec);
    return result;
}

int audio_codec_delete_ctrl_if(const audio_codec_ctrl_if_t *control)
{
    if (!control) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int result = control->close ? control->close(control) : ESP_CODEC_DEV_OK;
    free((void *)control);
    return result;
}

int audio_codec_delete_gpio_if(const audio_codec_gpio_if_t *gpio)
{
    if (!gpio) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    free((void *)gpio);
    return ESP_CODEC_DEV_OK;
}
