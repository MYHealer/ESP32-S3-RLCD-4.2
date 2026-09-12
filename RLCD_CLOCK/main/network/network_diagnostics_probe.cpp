// 执行网络检测底层 DNS、HTTP 和公网 IP 探测，不维护页面状态。
#include "network_diagnostics_probe.h"

#include "network_http_client.h"
#include "network_public_ip_parser.h"
#include "scoped_heap_buffer.h"

#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_text_format.h"

#include "esp_log.h"
#include "lwip/netdb.h"

#define NETWORK_DIAG_RESPONSE_ALLOC_FAILED_FORMAT \
    "network diag response alloc failed len=%u"
#define NETWORK_DIAG_DNS_INVALID_HOST_LOG "network diag dns invalid host"
#define NETWORK_DIAG_DNS_LOOKUP_FAILED_FORMAT \
    "network diag dns lookup failed host=%s rc=%d"
#define NETWORK_DIAG_HTTP_PROBE_INVALID_ARG_LOG \
    "network diag http probe invalid arg"
#define NETWORK_DIAG_PUBLIC_IP_PARSE_FAILED_LOG \
    "network diag public ip parse failed"
#define NETWORK_DIAG_PUBLIC_IP_HTTP_FAILED_LOG \
    "network diag public ip http failed"

namespace {
constexpr bool http_probe_args_valid(const char *url, size_t buffer_len)
{
    return cstr_nonempty(url) && buffer_len > 0;
}
} // namespace

bool network_diagnostic_dns_lookup_ok(const char *host)
{
    if (!host || host[0] == '\0') {
        ESP_LOGW(TAG, "%s", NETWORK_DIAG_DNS_INVALID_HOST_LOG);
        return false;
    }
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *result = nullptr;
    const int rc = getaddrinfo(host, nullptr, &hints, &result);
    const bool ok = network_diagnostic_dns_result_ok(rc, result != nullptr);
    if (result) {
        freeaddrinfo(result);
    }
    if (!ok) {
        ESP_LOGW(TAG, NETWORK_DIAG_DNS_LOOKUP_FAILED_FORMAT, host, rc);
    }
    return ok;
}

bool network_diagnostic_http_probe_ok(const char *url, size_t buffer_len)
{
    if (!http_probe_args_valid(url, buffer_len)) {
        ESP_LOGW(TAG, "%s", NETWORK_DIAG_HTTP_PROBE_INVALID_ARG_LOG);
        return false;
    }
    ScopedHeapBuffer<char> response(buffer_len,
                                    HeapBufferInit::kZeroed,
                                    HeapBufferStorage::kPsramPreferred);
    if (!response) {
        ESP_LOGW(TAG,
                 NETWORK_DIAG_RESPONSE_ALLOC_FAILED_FORMAT,
                 static_cast<unsigned>(buffer_len));
        return false;
    }
    return http_get_text(url, response.get(), response.size(), nullptr) == ESP_OK;
}

NetworkDiagnosticPublicIpLookupResult network_diagnostic_lookup_public_ip(
    const char *url,
    char *out,
    size_t out_len,
    size_t response_buffer_len)
{
    if (!http_probe_args_valid(url, response_buffer_len) ||
        !app_text::output_buffer_available(out, out_len)) {
        return {};
    }
    out[0] = '\0';
    ScopedHeapBuffer<char> response(response_buffer_len,
                                    HeapBufferInit::kZeroed,
                                    HeapBufferStorage::kPsramPreferred);
    if (!response) {
        ESP_LOGW(TAG,
                 NETWORK_DIAG_RESPONSE_ALLOC_FAILED_FORMAT,
                 static_cast<unsigned>(response_buffer_len));
        return {};
    }
    NetworkDiagnosticPublicIpLookupResult result = {};
    if (http_get_text(url,
                      response.get(),
                      response.size(),
                      nullptr) == ESP_OK) {
        result.request_ok = true;
        result.address_ok = network_public_ip_parse_response(response.get(),
                                                             out,
                                                             out_len);
        if (!result.address_ok) {
            ESP_LOGW(TAG, "%s", NETWORK_DIAG_PUBLIC_IP_PARSE_FAILED_LOG);
        }
    } else {
        ESP_LOGW(TAG, "%s", NETWORK_DIAG_PUBLIC_IP_HTTP_FAILED_LOG);
    }
    return result;
}
