// 验证温湿时钟舒适、还好和偏差表情分级的闭区间边界。
#include "ui_flip_sensor_mood.h"

#include <assert.h>
#include <limits>

int main()
{
    assert(temperature_mood(17.9f) == kSensorMoodBad);
    assert(temperature_mood(18.0f) == kSensorMoodOk);
    assert(temperature_mood(19.9f) == kSensorMoodOk);
    assert(temperature_mood(20.0f) == kSensorMoodComfort);
    assert(temperature_mood(26.0f) == kSensorMoodComfort);
    assert(temperature_mood(26.1f) == kSensorMoodOk);
    assert(temperature_mood(30.0f) == kSensorMoodOk);
    assert(temperature_mood(30.1f) == kSensorMoodBad);

    assert(humidity_mood(29.9f) == kSensorMoodBad);
    assert(humidity_mood(30.0f) == kSensorMoodOk);
    assert(humidity_mood(39.9f) == kSensorMoodOk);
    assert(humidity_mood(40.0f) == kSensorMoodComfort);
    assert(humidity_mood(60.0f) == kSensorMoodComfort);
    assert(humidity_mood(60.1f) == kSensorMoodOk);
    assert(humidity_mood(70.0f) == kSensorMoodOk);
    assert(humidity_mood(70.1f) == kSensorMoodBad);

    const SensorComfortBand custom_band = {2.0f, 4.0f, 1.0f, 5.0f};
    assert(classify_sensor_mood(3.0f, custom_band) == kSensorMoodComfort);
    assert(classify_sensor_mood(1.0f, custom_band) == kSensorMoodOk);
    assert(classify_sensor_mood(6.0f, custom_band) == kSensorMoodBad);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(temperature_mood(nan) == kSensorMoodBad);
    assert(humidity_mood(nan) == kSensorMoodBad);
    return 0;
}
