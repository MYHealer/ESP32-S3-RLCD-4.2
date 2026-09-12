// 实现配网 HTTP 路由、表单保存和强制门户服务生命周期。
#include "wifi_portal_http.h"

#include "network_provisioning.h"

#include "app_constexpr.h"
#include "app_event_group.h"
#include "app_metadata.h"
#include "network_diagnostics_state.h"
#include "network_form.h"
#include "ascii_text.h"
#include "setup_portal_control.h"
#include "wifi_portal_dns.h"
#include "wifi_portal_pages.h"
#include "wifi_portal_state_internal.h"

#include "ui_info_page_state.h"
#include "ui_settings_activity_state.h"
#include "ui_task_notify.h"

#include "display_bsp.h"

#include <esp_attr.h>
#include <esp_log.h>
#include <string.h>

namespace {
httpd_handle_t s_http_server = nullptr;
bool s_http_routes_ready = false;
bool s_display_dma_guard_active = false;
constexpr uint16_t kSetupHttpServerPort = 80;
constexpr size_t kSetupHttpServerStackSize = 8192;
constexpr size_t kSetupHttpMaxRequestHeaderLength = 1024;
constexpr size_t kPortalSubmitSsidFieldSize = 33;
constexpr size_t kPortalRequestBufferSize = 1536;
// ESP-IDF invokes synchronous URI handlers on the single HTTP server task.
// Keep their mutually exclusive request staging area off that task's stack.
EXT_RAM_BSS_ATTR char s_portal_request_buffer[kPortalRequestBufferSize];
constexpr uint32_t kPortalResponseSettleMs = 750;
constexpr const char *kPortalHttpStatusBadRequest = "400 Bad Request";
constexpr const char *kPortalHttpStatusPayloadTooLarge = "413 Payload Too Large";
constexpr const char *kPortalHttpStatusOk = "200 OK";
constexpr const char *kPortalHttpStatusConflict = "409 Conflict";
constexpr const char *kPortalHttpStatusNoContent = "204 No Content";
constexpr const char *kPortalErrorMissingQuery = "缺少请求参数。";
constexpr const char *kPortalErrorRequestTooLarge = "提交内容过长，请缩短自定义字段后重试。";
constexpr const char *kPortalRootUri = "/";
constexpr const char *kPortalSaveUri = "/save";
constexpr const char *kPortalStatusUri = "/status";
constexpr const char *kPortalFaviconUri = "/favicon.ico";
constexpr const char *kPortalAppleTouchIconUri = "/apple-touch-icon.png";
constexpr const char *kPortalAppleTouchIconPrecomposedUri = "/apple-touch-icon-precomposed.png";
constexpr const char *kPortalWildcardUri = "/*";

esp_err_t save_post_handler(httpd_req_t *req);
esp_err_t save_get_handler(httpd_req_t *req);
esp_err_t portal_status_get_handler(httpd_req_t *req);
esp_err_t empty_asset_handler(httpd_req_t *req);
esp_err_t captive_portal_handler(httpd_req_t *req);

struct PortalHttpRoute {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
};

constexpr PortalHttpRoute kPortalHttpRoutes[] = {
    {kPortalRootUri, HTTP_GET, root_get_handler},
    {kPortalSaveUri, HTTP_POST, save_post_handler},
    {kPortalSaveUri, HTTP_GET, save_get_handler},
    {kPortalStatusUri, HTTP_GET, portal_status_get_handler},
    {kPortalFaviconUri, HTTP_GET, empty_asset_handler},
    {kPortalAppleTouchIconUri, HTTP_GET, empty_asset_handler},
    {kPortalAppleTouchIconPrecomposedUri, HTTP_GET, empty_asset_handler},
    {kPortalWildcardUri, HTTP_GET, captive_portal_handler},
};

constexpr bool portal_http_routes_valid()
{
    for (const PortalHttpRoute &route : kPortalHttpRoutes) {
        if (!cstr_nonempty(route.uri) || !route.handler) {
            return false;
        }
    }
    return true;
}

static_assert(kSetupHttpServerPort > 0, "setup HTTP server port must be positive");
static_assert(kSetupHttpServerStackSize > 0, "setup HTTP server stack must be positive");
static_assert(kSetupHttpMaxRequestHeaderLength >= 1024,
              "setup HTTP request headers must support modern captive browsers");
static_assert(kSetupHttpMaxRequestHeaderLength <= 2048,
              "setup HTTP request headers must keep bounded parser memory");
static_assert(kPortalRequestBufferSize > kPortalSubmitSsidFieldSize,
              "portal request buffer must exceed submitted SSID field size");
static_assert(sizeof(s_portal_request_buffer) == kPortalRequestBufferSize,
              "portal request workspace must match request buffer capacity");
static_assert(kPortalResponseSettleMs > 0,
              "portal response settle delay must be positive");
static_assert(array_count(kPortalHttpRoutes) > 0, "portal HTTP route table must not be empty");
static_assert(portal_http_routes_valid(), "portal HTTP routes must have URI and handler");

#define SETUP_PORTAL_WITHOUT_CAPTIVE_DNS_LOG "setup portal running without captive dns"
#define PORTAL_HTTP_SERVER_START_FAILED_FORMAT "http server start failed: %s"
#define PORTAL_HTTP_SERVER_STOP_FAILED_FORMAT "http server stop failed: %s"
#define PORTAL_HTTP_URI_REGISTER_FAILED_FORMAT "http uri register failed: %s"
#define PORTAL_POST_BODY_TRUNCATED_FORMAT "setup POST body truncated content_len=%u buffer=%u"
#define PORTAL_POST_BODY_RECEIVE_FAILED_FORMAT "setup POST body receive failed ret=%d received=%d expected=%u"
#define PORTAL_PROVISIONING_SYNC_EVENT_UNAVAILABLE_LOG "setup save skipped initial sync request: app events unavailable"
#define PORTAL_OFFLINE_STOP_REQUEST_UNAVAILABLE_LOG "offline setup could not queue portal stop"

bool request_provisioning_sync_after_save()
{
    if (!app_event_group_ready()) {
        ESP_LOGW(TAG, "%s", PORTAL_PROVISIONING_SYNC_EVENT_UNAVAILABLE_LOG);
        return false;
    }
    app_event_group_set_bits(kProvisioningSyncBit);
    return true;
}

void acquire_portal_display_dma_guard()
{
    if (s_display_dma_guard_active) {
        return;
    }
    Display_AcquireDmaConservativeMode();
    s_display_dma_guard_active = true;
}

void release_portal_display_dma_guard()
{
    if (!s_display_dma_guard_active) {
        return;
    }
    Display_ReleaseDmaConservativeMode();
    s_display_dma_guard_active = false;
}

bool stop_http_server_handle()
{
    if (!s_http_server) {
        s_http_routes_ready = false;
        release_portal_display_dma_guard();
        return true;
    }
    esp_err_t err = httpd_stop(s_http_server);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, PORTAL_HTTP_SERVER_STOP_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    s_http_server = nullptr;
    s_http_routes_ready = false;
    release_portal_display_dma_guard();
    return true;
}

esp_err_t handle_setup_save(httpd_req_t *req, const char *body)
{
    if (app_event_group_ready()) {
        // Retire any older level-triggered validation before publishing the
        // next save generation. A worker that already captured the old request
        // will reject itself through the generation check.
        app_event_group_clear_bits(kProvisioningSyncBit);
    }
    wifi_portal_begin_save_attempt();
    char ssid[kPortalSubmitSsidFieldSize] = {};
    form_value(body, "ssid", ssid, sizeof(ssid));
    trim_ascii_whitespace(ssid);
    if (ssid[0] == '\0') {
        bool offline_saved = save_offline_datetime_from_body(body);
        if (!offline_saved) {
            wifi_portal_save_result_store(WifiPortalSaveResult::kInvalidInput);
        }
        esp_err_t err = send_offline_result_page(req, offline_saved);
        if (offline_saved) {
            settings_page_clear();
            network_diag_page_clear();
            info_page_clear();
            if (err == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(kPortalResponseSettleMs));
            }
            if (!request_setup_portal_stop()) {
                ESP_LOGW(TAG, "%s", PORTAL_OFFLINE_STOP_REQUEST_UNAVAILABLE_LOG);
            }
            notify_ui_task();
        }
        return err;
    }
    const bool saved = save_credentials_from_body(body);
    WifiPortalSaveResult result = saved
                                      ? WifiPortalSaveResult::kValidating
                                      : WifiPortalSaveResult::kInvalidInput;
    wifi_portal_save_result_store(result);
    esp_err_t err = send_save_result_page(req, result);
    if (saved) {
        // httpd_resp_send() has handed the body to lwIP, but changing an APSTA
        // channel immediately afterwards can still disconnect the phone before
        // its captive browser renders the result page.
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(kPortalResponseSettleMs));
        }
        if (!request_provisioning_sync_after_save()) {
            wifi_portal_save_result_store(WifiPortalSaveResult::kWifiConnectionFailed);
        }
    }
    return err;
}

esp_err_t receive_portal_post_body(httpd_req_t *req, char *body, size_t body_size)
{
    if (!req || !body || body_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    if (req->content_len >= body_size) {
        body[0] = '\0';
        ESP_LOGW(TAG,
                 PORTAL_POST_BODY_TRUNCATED_FORMAT,
                 (unsigned)req->content_len,
                 (unsigned)body_size);
        return ESP_ERR_INVALID_SIZE;
    }
    int total = 0;
    const int capacity = (int)body_size - 1;
    while (total < req->content_len) {
        int ret = httpd_req_recv(req, body + total, capacity - total);
        if (ret <= 0) {
            ESP_LOGW(TAG,
                     PORTAL_POST_BODY_RECEIVE_FAILED_FORMAT,
                     ret,
                     total,
                     (unsigned)req->content_len);
            return ESP_FAIL;
        }
        total += ret;
    }
    body[total] = '\0';
    return ESP_OK;
}

esp_err_t register_http_handler(httpd_handle_t server,
                                const char *uri,
                                httpd_method_t method,
                                esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    return httpd_register_uri_handler(server, &route);
}
} // namespace

bool stop_http_server()
{
    const bool http_stopped = stop_http_server_handle();
    stop_captive_dns_server();
    if (!http_stopped) {
        return false;
    }
    setup_portal_active_store(false);
    return true;
}

namespace {
esp_err_t save_post_handler(httpd_req_t *req)
{
    memset(s_portal_request_buffer, 0, sizeof(s_portal_request_buffer));
    esp_err_t err = receive_portal_post_body(req,
                                             s_portal_request_buffer,
                                             sizeof(s_portal_request_buffer));
    if (err == ESP_ERR_INVALID_SIZE) {
        return send_portal_text_status(req,
                                       kPortalHttpStatusPayloadTooLarge,
                                       kPortalErrorRequestTooLarge);
    }
    if (err != ESP_OK) {
        return err;
    }
    return handle_setup_save(req, s_portal_request_buffer);
}

esp_err_t save_get_handler(httpd_req_t *req)
{
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(s_portal_request_buffer, 0, sizeof(s_portal_request_buffer));
    if (httpd_req_get_url_query_str(req,
                                    s_portal_request_buffer,
                                    sizeof(s_portal_request_buffer)) != ESP_OK) {
        return send_portal_text_status(req, kPortalHttpStatusBadRequest, kPortalErrorMissingQuery);
    }
    return handle_setup_save(req, s_portal_request_buffer);
}

esp_err_t portal_status_get_handler(httpd_req_t *req)
{
    const WifiPortalSaveSnapshot save =
        wifi_portal_save_snapshot_load();
    const WifiPortalSaveResult result = save.result;
    if (result == WifiPortalSaveResult::kSuccess) {
        esp_err_t err = send_portal_empty_status(req, kPortalHttpStatusOk);
        if (err == ESP_OK) {
            (void)wifi_portal_mark_save_feedback_seen(save);
        }
        return err;
    }
    if (result != WifiPortalSaveResult::kNone &&
        result != WifiPortalSaveResult::kValidating) {
        return send_portal_empty_status(req, kPortalHttpStatusConflict);
    }
    return send_portal_empty_status(req, kPortalHttpStatusNoContent);
}

esp_err_t empty_asset_handler(httpd_req_t *req)
{
    return send_portal_empty_status(req, kPortalHttpStatusNoContent);
}

esp_err_t captive_portal_handler(httpd_req_t *req)
{
    return redirect_to_setup_portal(req);
}
} // namespace

bool start_http_server()
{
    if (s_http_server &&
        (!s_http_routes_ready || !setup_portal_active_load()) &&
        !stop_http_server_handle()) {
        return false;
    }
    if (s_http_server) {
        acquire_portal_display_dma_guard();
        setup_portal_active_store(true);
        if (!start_captive_dns_server()) {
            ESP_LOGW(TAG, SETUP_PORTAL_WITHOUT_CAPTIVE_DNS_LOG);
        }
        return true;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kSetupHttpServerPort;
    config.stack_size = kSetupHttpServerStackSize;
    // The project-wide 512-byte default is too small for some modern captive
    // browsers once User-Agent, language and portal-probe headers are combined.
    // Scope the official ESP-IDF 1024-byte default to this temporary server.
    config.max_req_hdr_len = kSetupHttpMaxRequestHeaderLength;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        s_http_server = nullptr;
        s_http_routes_ready = false;
        setup_portal_active_store(false);
        ESP_LOGW(TAG, PORTAL_HTTP_SERVER_START_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    acquire_portal_display_dma_guard();

    for (const PortalHttpRoute &route : kPortalHttpRoutes) {
        if (err != ESP_OK) {
            break;
        }
        err = register_http_handler(s_http_server, route.uri, route.method, route.handler);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, PORTAL_HTTP_URI_REGISTER_FAILED_FORMAT, esp_err_to_name(err));
        const bool stopped = stop_http_server_handle();
        setup_portal_active_store(!stopped);
        if (!stopped) {
            (void)request_setup_portal_stop();
        }
        return false;
    }
    s_http_routes_ready = true;
    if (!start_captive_dns_server()) {
        ESP_LOGW(TAG, SETUP_PORTAL_WITHOUT_CAPTIVE_DNS_LOG);
    }
    setup_portal_active_store(true);
    return true;
}
