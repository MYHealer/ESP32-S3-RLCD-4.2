// 实现不依赖采样任务和 UI 的温湿度趋势窗口纯计算。
#include "sensor_trend.h"

namespace {
bool sensor_sample_in_trend_window(const SensorSample &sample,
                                   int64_t now_ms,
                                   int64_t window_ms)
{
    return window_ms > 0 &&
           sample.sampled_at_ms > 0 &&
           sample.sampled_at_ms >= now_ms - window_ms &&
           sample.sampled_at_ms <= now_ms;
}

int trend_direction(float delta, float epsilon)
{
    return delta > epsilon ? 1 : (delta < -epsilon ? -1 : 0);
}
} // namespace

SensorTrendAverage calculate_sensor_trend_average(const SensorSample *samples,
                                                  int sample_count,
                                                  int sample_capacity,
                                                  int64_t now_ms,
                                                  int64_t window_ms)
{
    SensorTrendAverage average = {};
    if (!samples || sample_count <= 0 || sample_capacity <= 0 || window_ms <= 0) {
        return average;
    }
    if (sample_count > sample_capacity) {
        sample_count = sample_capacity;
    }
    float temperature_sum = 0.0f;
    float humidity_sum = 0.0f;
    for (int i = 0; i < sample_count; ++i) {
        if (!sensor_sample_in_trend_window(samples[i], now_ms, window_ms)) {
            continue;
        }
        temperature_sum += samples[i].temperature;
        humidity_sum += samples[i].humidity;
        ++average.count;
    }
    if (average.count > 0) {
        average.temperature = temperature_sum / average.count;
        average.humidity = humidity_sum / average.count;
    }
    return average;
}

void calculate_sensor_trend_directions(const SensorTrendAverage &average,
                                       bool previous_average_valid,
                                       float previous_temperature_average,
                                       float previous_humidity_average,
                                       float epsilon,
                                       int *temperature_trend,
                                       int *humidity_trend)
{
    if (!temperature_trend || !humidity_trend) {
        return;
    }
    *temperature_trend = 0;
    *humidity_trend = 0;
    if (!previous_average_valid || average.count < 2) {
        return;
    }
    *temperature_trend = trend_direction(
        average.temperature - previous_temperature_average,
        epsilon);
    *humidity_trend = trend_direction(
        average.humidity - previous_humidity_average,
        epsilon);
}
