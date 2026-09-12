// 为不检查日志内容的主机测试提供无副作用 ESP 日志桩。
#pragma once

template <typename... Args>
inline void host_log_warning(Args...)
{
}

#define ESP_LOGW(...) host_log_warning(__VA_ARGS__)
