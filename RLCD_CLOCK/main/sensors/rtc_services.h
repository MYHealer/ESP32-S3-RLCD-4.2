// 声明 RTC 时间恢复与系统时间回写的窄服务接口。
#pragma once

void restore_system_time_from_rtc();
void sync_rtc_from_system_time();
