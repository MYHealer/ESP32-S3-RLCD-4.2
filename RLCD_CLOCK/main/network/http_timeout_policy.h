// 定义普通 HTTP 请求固定超时，并按启动联网剩余预算进行纯计算裁剪。
#pragma once

inline constexpr int kNetworkHttpDefaultTimeoutMs = 10000;

constexpr int network_http_timeout_for_budget(int remaining_ms)
{
    if (remaining_ms <= 0) {
        return 0;
    }
    return remaining_ms < kNetworkHttpDefaultTimeoutMs
               ? remaining_ms
               : kNetworkHttpDefaultTimeoutMs;
}

static_assert(kNetworkHttpDefaultTimeoutMs > 0,
              "default HTTP timeout must be positive");
static_assert(network_http_timeout_for_budget(0) == 0,
              "exhausted startup budget must reject HTTP work");
static_assert(network_http_timeout_for_budget(kNetworkHttpDefaultTimeoutMs + 1) ==
                  kNetworkHttpDefaultTimeoutMs,
              "large startup budget must retain the default HTTP timeout");
