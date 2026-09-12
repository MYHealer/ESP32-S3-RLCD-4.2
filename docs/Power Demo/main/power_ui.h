#pragma once

#include <stdint.h>
#include <time.h>

class DisplayPort;

struct PowerSensorReading {
    bool available;
    float temperature;
    float humidity;
};

inline constexpr uint32_t kPowerUiSnapshotMagic = 0x50575231U;

struct PowerUiSnapshot {
    uint32_t magic;
    int date_key;
    int am_pm;
    int hour12;
    int minute;
    int temperature_tenths;
    int humidity_percent;
    int battery_segments;
    int progress_filled;
    bool sound_enabled;
};

PowerUiSnapshot power_ui_snapshot(const struct tm &local,
                                  const PowerSensorReading &sensor,
                                  int battery_percent,
                                  bool sound_enabled);

void power_ui_render(DisplayPort &display,
                     const struct tm &local,
                     const PowerSensorReading &sensor,
                     int battery_percent,
                     bool sound_enabled);

void power_ui_refresh(DisplayPort &display,
                      const PowerUiSnapshot &previous,
                      const PowerUiSnapshot &current,
                      bool force_full);
