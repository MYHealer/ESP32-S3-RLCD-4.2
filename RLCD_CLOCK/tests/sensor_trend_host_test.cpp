// 验证四小时趋势窗口、平均值和箭头方向判定的边界行为。
#include "sensor_trend.h"

#include <assert.h>
#include <math.h>
#include <limits>

namespace {
bool near_value(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001f;
}
}

int main()
{
    constexpr int64_t now_ms = 10000;
    constexpr int64_t window_ms = 4000;
    SensorSample samples[] = {
        {0, 99.0f, 99.0f},
        {now_ms - window_ms - 1, 88.0f, 88.0f},
        {now_ms - window_ms, 20.0f, 40.0f},
        {now_ms - 1000, 22.0f, 50.0f},
        {now_ms, 24.0f, 60.0f},
        {now_ms + 1, 77.0f, 77.0f},
    };
    SensorTrendAverage average = calculate_sensor_trend_average(
        samples,
        6,
        6,
        now_ms,
        window_ms);
    assert(average.count == 3);
    assert(near_value(average.temperature, 22.0f));
    assert(near_value(average.humidity, 50.0f));

    average = calculate_sensor_trend_average(samples, 100, 4, now_ms, window_ms);
    assert(average.count == 2);
    assert(near_value(average.temperature, 21.0f));
    assert(near_value(average.humidity, 45.0f));
    assert(calculate_sensor_trend_average(nullptr, 6, 6, now_ms, window_ms).count == 0);
    assert(calculate_sensor_trend_average(samples, 0, 6, now_ms, window_ms).count == 0);
    assert(calculate_sensor_trend_average(samples, 6, 6, now_ms, 0).count == 0);

    int temperature_trend = 9;
    int humidity_trend = 9;
    calculate_sensor_trend_directions(average,
                                      false,
                                      20.0f,
                                      40.0f,
                                      1.0f,
                                      &temperature_trend,
                                      &humidity_trend);
    assert(temperature_trend == 0 && humidity_trend == 0);

    average.count = 1;
    calculate_sensor_trend_directions(average,
                                      true,
                                      20.0f,
                                      40.0f,
                                      1.0f,
                                      &temperature_trend,
                                      &humidity_trend);
    assert(temperature_trend == 0 && humidity_trend == 0);

    average.count = 2;
    average.temperature = 22.0f;
    average.humidity = 38.0f;
    calculate_sensor_trend_directions(average,
                                      true,
                                      20.0f,
                                      40.0f,
                                      1.0f,
                                      &temperature_trend,
                                      &humidity_trend);
    assert(temperature_trend == 1 && humidity_trend == -1);

    average.temperature = 21.0f;
    average.humidity = 39.0f;
    calculate_sensor_trend_directions(average,
                                      true,
                                      20.0f,
                                      40.0f,
                                      1.0f,
                                      &temperature_trend,
                                      &humidity_trend);
    assert(temperature_trend == 0 && humidity_trend == 0);

    average.temperature = std::numeric_limits<float>::quiet_NaN();
    average.humidity = std::numeric_limits<float>::quiet_NaN();
    calculate_sensor_trend_directions(average,
                                      true,
                                      20.0f,
                                      40.0f,
                                      1.0f,
                                      &temperature_trend,
                                      &humidity_trend);
    assert(temperature_trend == 0 && humidity_trend == 0);
    calculate_sensor_trend_directions(average, true, 0.0f, 0.0f, 1.0f, nullptr, nullptr);
    return 0;
}
