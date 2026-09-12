// 声明本地温湿度四小时趋势使用的纯窗口平均与方向计算。
#pragma once

#include "sensor_history_types.h"

#include <stdint.h>

struct SensorTrendAverage {
    float temperature = 0.0f;
    float humidity = 0.0f;
    int count = 0;
};

SensorTrendAverage calculate_sensor_trend_average(const SensorSample *samples,
                                                  int sample_count,
                                                  int sample_capacity,
                                                  int64_t now_ms,
                                                  int64_t window_ms);
void calculate_sensor_trend_directions(const SensorTrendAverage &average,
                                       bool previous_average_valid,
                                       float previous_temperature_average,
                                       float previous_humidity_average,
                                       float epsilon,
                                       int *temperature_trend,
                                       int *humidity_trend);
