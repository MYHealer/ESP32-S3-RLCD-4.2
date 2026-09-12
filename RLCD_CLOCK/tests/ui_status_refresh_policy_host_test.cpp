// 验证工作页状态栏按输入变化刷新和低频兜底规则。
#include "ui_status_refresh_policy.h"

#include <assert.h>

int main()
{
    assert(ui_weather_network_status_required(true, false, false));
    assert(!ui_weather_network_status_required(false, false, false));
    assert(!ui_weather_network_status_required(true, true, false));
    assert(!ui_weather_network_status_required(true, false, true));

    UiStatusRefreshSnapshot stable = {4, 0, false, false, false};
    assert(ui_status_refresh_due(stable, stable, false, false));
    assert(!ui_status_refresh_due(stable, stable, true, false));
    assert(ui_status_refresh_due(stable, stable, true, true));
    assert(ui_sensor_status_refresh_due(stable, stable, false, false));
    assert(!ui_sensor_status_refresh_due(stable, stable, true, false));
    assert(ui_sensor_status_refresh_due(stable, stable, true, true));

    UiStatusRefreshSnapshot changed = stable;
    ++changed.sensor_version;
    assert(ui_status_refresh_inputs_changed(changed, stable));
    assert(ui_status_refresh_due(changed, stable, true, false));
    assert(ui_sensor_status_refresh_due(changed, stable, true, false));

    changed = stable;
    changed.weather_network_bits = 1;
    assert(ui_status_refresh_due(changed, stable, true, false));
    assert(!ui_sensor_status_refresh_due(changed, stable, true, false));

    changed = stable;
    changed.chime_enabled = true;
    assert(ui_status_refresh_due(changed, stable, true, false));
    assert(!ui_sensor_status_refresh_due(changed, stable, true, false));

    changed = stable;
    changed.wifi_radio_on = true;
    assert(ui_status_refresh_due(changed, stable, true, false));
    assert(!ui_sensor_status_refresh_due(changed, stable, true, false));

    changed = stable;
    changed.alarm_enabled = true;
    assert(ui_status_refresh_due(changed, stable, true, false));
    assert(!ui_sensor_status_refresh_due(changed, stable, true, false));
    return 0;
}
