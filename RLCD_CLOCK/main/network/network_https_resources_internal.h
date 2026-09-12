// 声明网络同步内部使用的 HTTPS 内存余量采样接口。
#pragma once

#include <stddef.h>

struct NetworkHttpsMemorySnapshot {
    size_t internal_free = 0;
    size_t internal_largest = 0;
    size_t dma_largest = 0;
    bool sufficient = false;
};

// 启动 HTTPS 前统一采样内部堆和 DMA 最大连续块；调用方保留各自日志与延后策略。
NetworkHttpsMemorySnapshot capture_network_https_memory_snapshot();
