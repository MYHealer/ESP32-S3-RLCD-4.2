// 统一定义网络检测结果行的稳定编号和边界校验。
#pragma once

enum NetworkDiagLineIndex : int {
    kNetworkDiagLocalIpLine = 0,
    kNetworkDiagPublicIpLine,
    kNetworkDiagIpLocationLine,
    kNetworkDiagDnsLine,
    kNetworkDiagWeatherLine,
    kNetworkDiagNtpLine,
    kNetworkDiagSayingLine,
    kNetworkDiagInternetLine,
    kNetworkDiagOtaLine,
    kNetworkDiagLineCount,
};

inline constexpr int kNetworkDiagLineLen = 48;

constexpr bool network_diag_line_index_valid(int index)
{
    return index >= kNetworkDiagLocalIpLine && index < kNetworkDiagLineCount;
}

static_assert(kNetworkDiagLocalIpLine == 0,
              "network diagnostics local IP line must remain first");
static_assert(kNetworkDiagOtaLine + 1 == kNetworkDiagLineCount,
              "network diagnostics OTA line must remain last");
static_assert(kNetworkDiagLineLen > 1,
              "network diagnostics lines must fit text and a NUL terminator");
