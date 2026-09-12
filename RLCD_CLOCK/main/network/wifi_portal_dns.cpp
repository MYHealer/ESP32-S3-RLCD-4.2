// 实现配网强制门户的 DHCP 选项和 UDP DNS 响应任务。
#include "wifi_portal_dns.h"

#include "atomic_ownership_gate.h"
#include "app_constexpr.h"
#include "app_metadata.h"
#include "app_network_config.h"
#include "captive_dns_packet.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <string.h>
#include <sys/time.h>

namespace {
AtomicTaskLifecycleGate s_captive_dns_task;
EXT_RAM_BSS_ATTR uint8_t s_captive_dns_packet[kCaptiveDnsPacketSize];
constexpr size_t kCaptivePortalUriSize = cstr_length(kSetupPortalUrl) + 1;
EXT_RAM_BSS_ATTR char s_captive_portal_uri[kCaptivePortalUriSize] = {};
constexpr uint16_t kCaptiveDnsPort = 53;
constexpr int kCaptiveDnsSocketTimeoutSec = 1;
constexpr int kCaptiveDnsStopWaitAttempts = 15;
constexpr uint32_t kCaptiveDnsStopWaitDelayMs = 100;
constexpr TickType_t kCaptiveDnsStopWaitDelay = pdMS_TO_TICKS(kCaptiveDnsStopWaitDelayMs);
constexpr int kCaptiveDnsRunAttempts = 3;
constexpr uint32_t kCaptiveDnsRunRetryDelayMs = 100;
constexpr TickType_t kCaptiveDnsRunRetryDelay =
    pdMS_TO_TICKS(kCaptiveDnsRunRetryDelayMs);
constexpr uint32_t kCaptiveDnsTaskStack = 2560;
constexpr UBaseType_t kCaptiveDnsTaskPriority = 3;
constexpr BaseType_t kCaptiveDnsTaskCore = 0;
constexpr const char *kCaptiveDnsTaskName = "captive_dns";

#define CAPTIVE_DNS_SOCKET_FAILED_LOG "captive dns socket failed"
#define CAPTIVE_DNS_BIND_FAILED_LOG "captive dns bind failed"
#define CAPTIVE_DNS_TIMEOUT_SETUP_FAILED_FORMAT "captive dns timeout setup failed errno=%d"
#define CAPTIVE_DNS_RECEIVE_FAILED_FORMAT "captive dns receive failed errno=%d"
#define CAPTIVE_DNS_STARTED_LOG "captive dns started"
#define CAPTIVE_DNS_STOPPED_LOG "captive dns stopped"
#define CAPTIVE_DHCPS_STOP_FAILED_FORMAT "dhcps stop before captive setup failed: %s"
#define CAPTIVE_DHCPS_DNS_OPTION_FAILED_FORMAT "dhcps dns option failed: %s"
#define CAPTIVE_AP_DNS_SETUP_FAILED_FORMAT "ap dns setup failed: %s"
#define CAPTIVE_DHCPS_URI_OPTION_FAILED_FORMAT "dhcps captive uri option failed: %s"
#define CAPTIVE_DHCPS_RESTART_FAILED_FORMAT "dhcps restart after captive setup failed: %s"
#define CAPTIVE_DHCPS_LEASE_RESET_STOP_FAILED_FORMAT "dhcps lease reset stop failed: %s"
#define CAPTIVE_DHCPS_LEASE_RESET_START_FAILED_FORMAT "dhcps lease reset start failed: %s"
#define CAPTIVE_DHCPS_LEASE_RESET_LOG "captive dhcp leases reset"
#define CAPTIVE_DNS_TASK_STILL_STOPPING_LOG "previous captive dns task still stopping"
#define CAPTIVE_DNS_TASK_START_FAILED_LOG "captive dns task start failed"
#define CAPTIVE_DNS_TASK_RETRY_FORMAT \
    "captive dns runtime failed; retrying attempt=%d/%d"
#define CAPTIVE_DNS_TASK_RETRY_EXHAUSTED_LOG \
    "captive dns runtime retry exhausted"

static_assert(kCaptiveDnsPort > 0, "captive DNS port must be positive");
static_assert(kCaptiveDnsSocketTimeoutSec > 0, "captive DNS socket timeout must be positive");
static_assert(kCaptiveDnsStopWaitAttempts > 0, "captive DNS stop wait attempts must be positive");
static_assert(kCaptiveDnsStopWaitDelayMs > 0, "captive DNS stop wait delay must be positive");
static_assert(kCaptiveDnsStopWaitDelay > 0, "captive DNS stop wait delay must be positive");
static_assert(kCaptiveDnsRunAttempts > 1,
              "captive DNS runtime must retain a recovery attempt");
static_assert(kCaptiveDnsRunRetryDelayMs > 0,
              "captive DNS runtime retry delay must be positive");
static_assert(kCaptiveDnsRunRetryDelay > 0,
              "captive DNS runtime retry tick delay must be positive");
static_assert(kCaptiveDnsTaskStack > 0, "captive DNS task stack must be positive");
static_assert(kCaptiveDnsTaskPriority > tskIDLE_PRIORITY, "captive DNS task priority must exceed idle");
static_assert(kCaptiveDnsTaskCore >= 0, "captive DNS task core must be non-negative");
static_assert(sizeof(s_captive_portal_uri) ==
                  cstr_length(kSetupPortalUrl) + 1,
              "captive portal URI storage must exactly fit URL and NUL");
static_assert(sizeof(s_captive_dns_packet) ==
                  static_cast<size_t>(kCaptiveDnsPacketSize),
              "captive DNS packet workspace must match packet capacity");
class ScopedSocketDescriptor {
public:
    explicit ScopedSocketDescriptor(int descriptor)
        : descriptor_(descriptor)
    {
    }

    ~ScopedSocketDescriptor()
    {
        if (descriptor_ >= 0) {
            close(descriptor_);
        }
    }

    ScopedSocketDescriptor(const ScopedSocketDescriptor &) = delete;
    ScopedSocketDescriptor &operator=(const ScopedSocketDescriptor &) = delete;

    int get() const
    {
        return descriptor_;
    }

    explicit operator bool() const
    {
        return descriptor_ >= 0;
    }

private:
    int descriptor_ = -1;
};

bool run_captive_dns_server()
{
    ScopedSocketDescriptor sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_IP));
    if (!sock) {
        ESP_LOGW(TAG, CAPTIVE_DNS_SOCKET_FAILED_LOG);
        return false;
    }

    timeval timeout = {};
    timeout.tv_sec = kCaptiveDnsSocketTimeoutSec;
    if (setsockopt(sock.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        ESP_LOGW(TAG, CAPTIVE_DNS_TIMEOUT_SETUP_FAILED_FORMAT, errno);
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kCaptiveDnsPort);
    if (bind(sock.get(), (sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW(TAG, CAPTIVE_DNS_BIND_FAILED_LOG);
        return false;
    }

    ESP_LOGI(TAG, CAPTIVE_DNS_STARTED_LOG);
    while (!s_captive_dns_task.stop_requested()) {
        memset(s_captive_dns_packet, 0, sizeof(s_captive_dns_packet));
        sockaddr_in from = {};
        socklen_t from_len = sizeof(from);
        int len = recvfrom(sock.get(),
                           s_captive_dns_packet,
                           sizeof(s_captive_dns_packet),
                           0,
                           (sockaddr *)&from,
                           &from_len);
        if (len < 0) {
            const int receive_errno = errno;
            if (receive_errno == EAGAIN ||
                receive_errno == EWOULDBLOCK ||
                receive_errno == EINTR) {
                continue;
            }
            ESP_LOGW(TAG,
                     CAPTIVE_DNS_RECEIVE_FAILED_FORMAT,
                     receive_errno);
            return false;
        }
        if (len == 0) {
            continue;
        }
        int response_len =
            build_captive_dns_response(s_captive_dns_packet,
                                       len,
                                       s_captive_dns_packet,
                                       sizeof(s_captive_dns_packet));
        if (response_len > 0) {
            sendto(sock.get(),
                   s_captive_dns_packet,
                   response_len,
                   0,
                   (sockaddr *)&from,
                   from_len);
        }
    }

    return true;
}

void captive_dns_task(void *)
{
    bool stopped_cleanly = false;
    for (int attempt = 0; attempt < kCaptiveDnsRunAttempts; ++attempt) {
        if (s_captive_dns_task.stop_requested()) {
            stopped_cleanly = true;
            break;
        }
        const bool run_stopped = run_captive_dns_server();
        if (run_stopped || s_captive_dns_task.stop_requested()) {
            stopped_cleanly = true;
            break;
        }
        if (attempt + 1 < kCaptiveDnsRunAttempts) {
            ESP_LOGW(TAG,
                     CAPTIVE_DNS_TASK_RETRY_FORMAT,
                     attempt + 2,
                     kCaptiveDnsRunAttempts);
            vTaskDelay(kCaptiveDnsRunRetryDelay);
        }
    }
    s_captive_dns_task.mark_stopped();
    if (stopped_cleanly) {
        ESP_LOGI(TAG, CAPTIVE_DNS_STOPPED_LOG);
    } else {
        ESP_LOGW(TAG, CAPTIVE_DNS_TASK_RETRY_EXHAUSTED_LOG);
    }
    vTaskDelete(nullptr);
}
} // namespace

void configure_captive_portal_dhcp(esp_netif_t *ap_netif)
{
    if (!ap_netif) {
        return;
    }
    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, CAPTIVE_DHCPS_STOP_FAILED_FORMAT, esp_err_to_name(err));
    }

    uint8_t offer_dns = 1;
    err = esp_netif_dhcps_option(ap_netif,
                                 ESP_NETIF_OP_SET,
                                 ESP_NETIF_DOMAIN_NAME_SERVER,
                                 &offer_dns,
                                 sizeof(offer_dns));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, CAPTIVE_DHCPS_DNS_OPTION_FAILED_FORMAT, esp_err_to_name(err));
    }

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(kSetupPortalIp);
    err = esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, CAPTIVE_AP_DNS_SETUP_FAILED_FORMAT, esp_err_to_name(err));
    }

    strlcpy(s_captive_portal_uri, kSetupPortalUrl, sizeof(s_captive_portal_uri));
    err = esp_netif_dhcps_option(ap_netif,
                                 ESP_NETIF_OP_SET,
                                 ESP_NETIF_CAPTIVEPORTAL_URI,
                                 s_captive_portal_uri,
                                 strlen(s_captive_portal_uri));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, CAPTIVE_DHCPS_URI_OPTION_FAILED_FORMAT, esp_err_to_name(err));
    }

    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG, CAPTIVE_DHCPS_RESTART_FAILED_FORMAT, esp_err_to_name(err));
    }
}

bool restart_captive_portal_dhcp(esp_netif_t *ap_netif)
{
    if (!ap_netif) {
        return false;
    }
    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG,
                 CAPTIVE_DHCPS_LEASE_RESET_STOP_FAILED_FORMAT,
                 esp_err_to_name(err));
        return false;
    }
    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG,
                 CAPTIVE_DHCPS_LEASE_RESET_START_FAILED_FORMAT,
                 esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "%s", CAPTIVE_DHCPS_LEASE_RESET_LOG);
    return true;
}

bool start_captive_dns_server()
{
    AtomicTaskStartClaim claim = s_captive_dns_task.try_begin_start();
    for (int attempt = 0;
         claim == AtomicTaskStartClaim::Stopping &&
         attempt < kCaptiveDnsStopWaitAttempts;
         ++attempt) {
        vTaskDelay(kCaptiveDnsStopWaitDelay);
        claim = s_captive_dns_task.try_begin_start();
    }
    if (claim == AtomicTaskStartClaim::AlreadyActive) {
        return true;
    }
    if (claim == AtomicTaskStartClaim::Stopping) {
        ESP_LOGW(TAG, CAPTIVE_DNS_TASK_STILL_STOPPING_LOG);
        return false;
    }
    // Publish the committed start before the new task can run and report Stopped.
    s_captive_dns_task.mark_running();
    BaseType_t ok = xTaskCreatePinnedToCore(captive_dns_task,
                                            kCaptiveDnsTaskName,
                                            kCaptiveDnsTaskStack,
                                            nullptr,
                                            kCaptiveDnsTaskPriority,
                                            nullptr,
                                            kCaptiveDnsTaskCore);
    if (ok != pdPASS) {
        s_captive_dns_task.mark_stopped();
        ESP_LOGW(TAG, CAPTIVE_DNS_TASK_START_FAILED_LOG);
        return false;
    }
    return true;
}

void stop_captive_dns_server()
{
    if (!s_captive_dns_task.request_stop()) {
        return;
    }
}
