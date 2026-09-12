// 声明天气时钟运行期时间和配网时钟刷新接口。
#pragma once

#include <time.h>

struct ClockUiTimeSnapshot;

bool update_time_ui(const struct tm &local,
                    const ClockUiTimeSnapshot &time_snapshot,
                    bool clock_page_active,
                    int active_work_page,
                    bool chime_enabled,
                    bool low_battery_mode,
                    bool setup_portal_active);
bool update_setup_clock_header_time_ui(const struct tm &local);
