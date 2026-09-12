// 声明 HTTPS 期间跨模块复用的显示 DMA 保护接口。
#pragma once

// TLS 握手期间切换到显示 DMA 保守模式，避免网络大块内存分配与 LCD
// 刷新争用内部 DMA 内存。天气、小智等联网模块复用同一个守卫。
class NetworkDisplayDmaGuard {
public:
    explicit NetworkDisplayDmaGuard(bool active);
    ~NetworkDisplayDmaGuard();

    NetworkDisplayDmaGuard(const NetworkDisplayDmaGuard &) = delete;
    NetworkDisplayDmaGuard &operator=(const NetworkDisplayDmaGuard &) = delete;

private:
    bool active_ = false;
};
