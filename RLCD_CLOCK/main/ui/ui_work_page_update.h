// 声明当前工作页时间、正文和天气预警的组合刷新入口。
#pragma once

#include "ui_status_refresh_policy.h"
#include "ui_visible_data_sync.h"

#include <stdint.h>
#include <time.h>

bool update_active_work_page_invalid_time_labels(int active_work_page,
                                                 bool force_weather_clock_status);
bool update_active_work_page_content(const struct tm &local,
                                     const ActiveWorkPageState &state,
                                     int active_page,
                                     bool status_due,
                                     const UiStatusRefreshSnapshot &status,
                                     bool low_battery_mode,
                                     bool setup_portal_active,
                                     bool &alert_visible,
                                     int &alert_index,
                                     uint32_t &alert_version);
