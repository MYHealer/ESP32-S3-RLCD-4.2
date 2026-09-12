// 为主机侧 MCP 协议测试声明最小设备接口，不参与固件构建。
#pragma once

#include <cJSON.h>
#include "battery_runtime_state.h"

#include <stddef.h>
#include <string.h>

#if !defined(__APPLE__)
static inline size_t strlcpy(char *destination, const char *source, size_t destination_size)
{
    size_t source_len = strlen(source);
    if (destination_size > 0) {
        size_t copy_len = source_len < destination_size - 1 ? source_len : destination_size - 1;
        memcpy(destination, source, copy_len);
        destination[copy_len] = '\0';
    }
    return source_len;
}
#endif

extern const char *const APP_VERSION;

bool get_local_sensor_snapshot(float *temperature,
                               float *humidity,
                               int *temperature_trend,
                               int *humidity_trend);
void apply_xiaozhi_speaker_volume(int volume_percent);
bool save_hourly_chime_setting();
