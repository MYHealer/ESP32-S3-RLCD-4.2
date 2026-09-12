// 定义温湿度趋势与小时历史共用的稳定数据类型和容量。
#pragma once

#include <stdint.h>

inline constexpr int kSensorHistoryMinutes = 240;
inline constexpr int kHourlyHistoryCount = 48;
inline constexpr int kLegacyHourlyHistoryCount = 24;
inline constexpr uint32_t kHourlyHistoryMagic = 0x48543234;
inline constexpr float kTrendEpsilon = 0.01f;

struct SensorSample {
    int64_t sampled_at_ms = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
};

struct HourlySensorSample {
    int64_t timestamp = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    uint8_t valid = 0;
    uint8_t reserved[7] = {};
};

struct HourlySensorHistoryBlob {
    uint32_t magic = kHourlyHistoryMagic;
    uint16_t version = 1;
    uint16_t count = kHourlyHistoryCount;
    HourlySensorSample samples[kHourlyHistoryCount] = {};
};
