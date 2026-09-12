// 验证网络检测行目录的稳定编号、连续顺序和边界判断。
#include "network_diagnostics_catalog.h"

#include <assert.h>

int main()
{
    constexpr int expected_lines[] = {
        kNetworkDiagLocalIpLine,
        kNetworkDiagPublicIpLine,
        kNetworkDiagIpLocationLine,
        kNetworkDiagDnsLine,
        kNetworkDiagWeatherLine,
        kNetworkDiagNtpLine,
        kNetworkDiagSayingLine,
        kNetworkDiagInternetLine,
        kNetworkDiagOtaLine,
    };
    static_assert(sizeof(expected_lines) / sizeof(expected_lines[0]) ==
                  static_cast<unsigned>(kNetworkDiagLineCount));

    for (int index = 0; index < kNetworkDiagLineCount; ++index) {
        assert(expected_lines[index] == index);
        assert(network_diag_line_index_valid(index));
    }
    assert(!network_diag_line_index_valid(-1));
    assert(!network_diag_line_index_valid(kNetworkDiagLineCount));
    assert(kNetworkDiagOtaLine == kNetworkDiagLineCount - 1);
    assert(kNetworkDiagLineLen == 48);
    return 0;
}
