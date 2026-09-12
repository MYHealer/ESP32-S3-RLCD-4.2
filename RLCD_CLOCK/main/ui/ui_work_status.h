// 声明各工作页共享状态栏的构建、刷新和传感器摘要接口。
#pragma once

#include "lvgl.h"

#include <time.h>

struct UiStatusRefreshSnapshot;

struct WorkPageStatusLabels {
    lv_obj_t *date;
    lv_obj_t *summary;
    lv_obj_t *time;
};

void build_work_page_status_bar(lv_obj_t *screen,
                                int page,
                                bool show_summary,
                                bool show_time);
WorkPageStatusLabels get_work_page_status_labels(int page);
bool update_work_page_status_time(int page, const struct tm &local);
bool update_non_clock_work_page_sensor_status(int page);
bool update_weather_clock_sensor_status();
bool update_work_page_status_icons(int page,
                                   const UiStatusRefreshSnapshot &status,
                                   bool low_battery_mode,
                                   bool setup_active);
