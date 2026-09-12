// 验证网络检测 DNS 返回码与地址结果的组合判定。
#include "network_diagnostics_probe.h"

#include <assert.h>

int main()
{
    assert(network_diagnostic_dns_result_ok(0, true));
    assert(!network_diagnostic_dns_result_ok(0, false));
    assert(!network_diagnostic_dns_result_ok(-1, true));
    assert(!network_diagnostic_dns_result_ok(-1, false));
    return 0;
}
