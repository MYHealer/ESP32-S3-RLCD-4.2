// 集中持有天气时钟构建与运行期刷新共享的 LVGL 对象引用。
#include "ui_clock.h"

#include "ui_clock_header_objects.h"
#include "ui_clock_sensor_objects.h"
#include "ui_clock_surface_objects.h"
#include "ui_clock_weather_panel_objects.h"
#include "ui_widgets.h"

#include <esp_attr.h>

namespace {
EXT_RAM_BSS_ATTR ClockWeatherPanelObjectRefs s_weather_panel_objects;
EXT_RAM_BSS_ATTR ClockLocalSensorObjectRefs s_local_sensor_objects;
EXT_RAM_BSS_ATTR ClockHeaderObjectRefs s_header_objects;
EXT_RAM_BSS_ATTR ClockSurfaceObjectRefs s_surface_objects;
} // namespace

ClockWeatherPanelObjectRefs &mutable_clock_weather_panel_object_refs()
{
    return s_weather_panel_objects;
}

bool set_clock_weather_panel_text(const char *city,
                                  const char *info,
                                  const char *temperature,
                                  const char *humidity,
                                  const char *icon_text)
{
    bool changed = set_label_text_if_changed(s_weather_panel_objects.city_label, city);
    changed |= set_label_text_if_changed(s_weather_panel_objects.info_label, info);
    changed |= set_label_text_if_changed(s_weather_panel_objects.temperature_label,
                                         temperature);
    changed |= set_label_text_if_changed(s_weather_panel_objects.humidity_label,
                                         humidity);
    changed |= set_label_text_if_changed(s_weather_panel_objects.icon_label, icon_text);
    return changed;
}

void clear_clock_weather_panel_object_refs()
{
    s_weather_panel_objects = {};
}

ClockLocalSensorObjectRefs &mutable_clock_local_sensor_object_refs()
{
    return s_local_sensor_objects;
}

const ClockLocalSensorObjectRefs &clock_local_sensor_object_refs()
{
    return s_local_sensor_objects;
}

void clear_clock_local_sensor_object_refs()
{
    s_local_sensor_objects = {};
}

ClockHeaderObjectRefs &mutable_clock_header_object_refs()
{
    return s_header_objects;
}

const ClockHeaderObjectRefs &clock_header_object_refs()
{
    return s_header_objects;
}

void clear_clock_header_object_refs()
{
    s_header_objects = {};
}

ClockSurfaceObjectRefs &mutable_clock_surface_object_refs()
{
    return s_surface_objects;
}

const ClockSurfaceObjectRefs &clock_surface_object_refs()
{
    return s_surface_objects;
}

void clear_clock_surface_object_refs()
{
    s_surface_objects = {};
}
