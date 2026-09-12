// 实现温湿时钟温度与湿度表情使用的固定舒适度分级。
#include "ui_flip_sensor_mood.h"

namespace {
constexpr float kComfortTempMinC = 20.0f;
constexpr float kComfortTempMaxC = 26.0f;
constexpr float kOkTempMinC = 18.0f;
constexpr float kOkTempMaxC = 30.0f;
constexpr float kComfortHumiMinPercent = 40.0f;
constexpr float kComfortHumiMaxPercent = 60.0f;
constexpr float kOkHumiMinPercent = 30.0f;
constexpr float kOkHumiMaxPercent = 70.0f;

constexpr SensorComfortBand kTemperatureBand = {
    kComfortTempMinC,
    kComfortTempMaxC,
    kOkTempMinC,
    kOkTempMaxC,
};
constexpr SensorComfortBand kHumidityBand = {
    kComfortHumiMinPercent,
    kComfortHumiMaxPercent,
    kOkHumiMinPercent,
    kOkHumiMaxPercent,
};

static_assert(kOkTempMinC <= kComfortTempMinC &&
                  kComfortTempMaxC <= kOkTempMaxC,
              "comfortable temperature range must stay inside ok temperature range");
static_assert(kOkHumiMinPercent <= kComfortHumiMinPercent &&
                  kComfortHumiMaxPercent <= kOkHumiMaxPercent,
              "comfortable humidity range must stay inside ok humidity range");
} // namespace

int classify_sensor_mood(float value, const SensorComfortBand &band)
{
    if (value >= band.comfort_min && value <= band.comfort_max) {
        return kSensorMoodComfort;
    }
    if (value >= band.ok_min && value <= band.ok_max) {
        return kSensorMoodOk;
    }
    return kSensorMoodBad;
}

int temperature_mood(float temperature)
{
    return classify_sensor_mood(temperature, kTemperatureBand);
}

int humidity_mood(float humidity)
{
    return classify_sensor_mood(humidity, kHumidityBand);
}
