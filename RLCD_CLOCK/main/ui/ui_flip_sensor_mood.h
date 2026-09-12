// 声明温湿时钟传感器舒适度分级的纯计算接口。
#pragma once

struct SensorComfortBand {
    float comfort_min;
    float comfort_max;
    float ok_min;
    float ok_max;
};

inline constexpr int kSensorMoodUnavailable = -1;
inline constexpr int kSensorMoodComfort = 0;
inline constexpr int kSensorMoodOk = 1;
inline constexpr int kSensorMoodBad = 2;

int classify_sensor_mood(float value, const SensorComfortBand &band);
int temperature_mood(float temperature);
int humidity_mood(float humidity);
