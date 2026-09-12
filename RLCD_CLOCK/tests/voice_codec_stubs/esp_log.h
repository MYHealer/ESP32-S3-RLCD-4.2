// 为小智编解码运行时主机测试屏蔽 ESP-IDF 日志宏。
#pragma once

#define ESP_LOGE(tag, format, ...) ((void)0)
