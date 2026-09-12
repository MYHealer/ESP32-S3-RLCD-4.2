// 声明网络检测内部使用的 DNS、HTTP 和公网 IP 探测能力。
#pragma once

#include <stddef.h>

struct NetworkDiagnosticPublicIpLookupResult {
    bool request_ok = false;
    bool address_ok = false;
};

constexpr bool network_diagnostic_dns_result_ok(int result_code,
                                                bool has_address)
{
    return result_code == 0 && has_address;
}

bool network_diagnostic_dns_lookup_ok(const char *host);
bool network_diagnostic_http_probe_ok(const char *url, size_t buffer_len);
NetworkDiagnosticPublicIpLookupResult network_diagnostic_lookup_public_ip(
    const char *url,
    char *out,
    size_t out_len,
    size_t response_buffer_len);
