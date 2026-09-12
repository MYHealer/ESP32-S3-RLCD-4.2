// 验证普通 HTTP 固定超时与启动联网剩余预算裁剪规则。
#include "http_timeout_policy.h"

#include <assert.h>
#include <limits.h>

int main()
{
    assert(network_http_timeout_for_budget(-1) == 0);
    assert(network_http_timeout_for_budget(0) == 0);
    assert(network_http_timeout_for_budget(1) == 1);
    assert(network_http_timeout_for_budget(2500) == 2500);
    assert(network_http_timeout_for_budget(kNetworkHttpDefaultTimeoutMs) ==
           kNetworkHttpDefaultTimeoutMs);
    assert(network_http_timeout_for_budget(kNetworkHttpDefaultTimeoutMs + 1) ==
           kNetworkHttpDefaultTimeoutMs);
    assert(network_http_timeout_for_budget(INT_MAX) ==
           kNetworkHttpDefaultTimeoutMs);
    return 0;
}
