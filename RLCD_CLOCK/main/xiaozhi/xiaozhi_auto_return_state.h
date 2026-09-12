// 声明小智自动返回开关的线程安全运行态访问接口。
#pragma once

inline constexpr bool kDefaultXiaozhiAutoReturnEnabled = true;

bool xiaozhi_auto_return_enabled_load();
