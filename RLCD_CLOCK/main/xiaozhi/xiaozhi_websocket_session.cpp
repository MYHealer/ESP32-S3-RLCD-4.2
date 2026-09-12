// 实现小智 WebSocket transport、事务锁和基础协议消息生命周期。
#include "xiaozhi_websocket_session.h"

#include "app_metadata.h"
#include "network_http_transaction_lock.h"
#include "xiaozhi_activation_client.h"
#include "xiaozhi_activation_storage.h"
#include "xiaozhi_protocol_utils.h"
#include "xiaozhi_server_hello_parser.h"

#include <cstring>

#include "esp_crt_bundle.h"
#include <esp_log.h>
#include "esp_transport_ssl.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ws.h"

namespace {
constexpr const char *kOfficialUserAgent = "ESP32-S3-RLCD-4.2/xiaozhi";
#define XIAOZHI_WEBSOCKET_OPTION_FAILED_FORMAT "xiaozhi websocket option %s failed: %s"

static_assert(xiaozhi_websocket::kTimeoutMs > 0,
              "Xiaozhi WebSocket timeout must be positive");
static_assert(sizeof(xiaozhi_websocket::WebsocketSession::session_id) > 1,
              "Xiaozhi session ID buffer must fit text and NUL");

bool websocket_option_set(esp_err_t err, const char *name)
{
    if (err == ESP_OK) {
        return true;
    }
    ESP_LOGW(TAG, XIAOZHI_WEBSOCKET_OPTION_FAILED_FORMAT, name, esp_err_to_name(err));
    return false;
}
} // namespace

namespace xiaozhi_websocket {

void close_websocket(WebsocketSession *session)
{
    if (!session) {
        return;
    }
    bool release_transaction_lock = session->network_transaction_locked;
    if (session->socket) {
        if (!session->peer_disconnected) {
            esp_transport_close(session->socket);
        }
        esp_transport_destroy(session->socket);
    }
    if (session->parent) {
        esp_transport_destroy(session->parent);
    }
    *session = {};
    if (release_transaction_lock) {
        release_network_http_transaction_lock();
    }
}

ScopedWebsocketSession::~ScopedWebsocketSession()
{
    reset();
}

void ScopedWebsocketSession::reset()
{
    close_websocket(&session_);
}

bool websocket_send_text(WebsocketSession *session, const char *text)
{
    return session && session->socket && text &&
           esp_transport_ws_send_raw(
               session->socket,
               static_cast<ws_transport_opcodes_t>(WS_TRANSPORT_OPCODES_FIN |
                                                   WS_TRANSPORT_OPCODES_TEXT),
               text,
               strlen(text),
               kTimeoutMs) >= 0;
}

bool websocket_send_listen_start(WebsocketSession *session)
{
    if (!session || session->session_id[0] == '\0') {
        return false;
    }
    char listen[128] = {};
    xiaozhi_protocol::format_listen_start(listen, sizeof(listen), session->session_id);
    return websocket_send_text(session, listen);
}

bool websocket_send_wake_abort(WebsocketSession *session)
{
    if (!session || session->session_id[0] == '\0') {
        return false;
    }
    char message[128] = {};
    xiaozhi_protocol::format_wake_abort(message, sizeof(message), session->session_id);
    return websocket_send_text(session, message);
}

bool suspend_websocket_transaction_lock(WebsocketSession *session)
{
    if (!session || !session->network_transaction_locked) {
        return false;
    }
    session->network_transaction_locked = false;
    release_network_http_transaction_lock();
    return true;
}

bool restore_websocket_transaction_lock(WebsocketSession *session, bool was_locked)
{
    if (!was_locked) {
        return true;
    }
    if (!session ||
        !acquire_network_http_transaction_lock(pdMS_TO_TICKS(kTimeoutMs))) {
        ESP_LOGW(TAG, "Xiaozhi failed to restore WebSocket transaction lock");
        return false;
    }
    session->network_transaction_locked = true;
    return true;
}

bool open_websocket(WebsocketSession *session, const char *url, const char *token, int version)
{
    if (!session) {
        return false;
    }
    bool secure = false;
    char host[128] = {};
    char path[256] = {};
    int port = 0;
    if (!xiaozhi_protocol::parse_websocket_url(
            url, &secure, host, sizeof(host), &port, path, sizeof(path))) {
        return false;
    }
    if (!acquire_network_http_transaction_lock(pdMS_TO_TICKS(kTimeoutMs))) {
        return false;
    }
    session->network_transaction_locked = true;
    session->parent = secure ? esp_transport_ssl_init() : esp_transport_tcp_init();
    if (!session->parent) {
        close_websocket(session);
        return false;
    }
    if (secure) {
        esp_transport_ssl_crt_bundle_attach(session->parent, esp_crt_bundle_attach);
    }
    session->socket = esp_transport_ws_init(session->parent);
    if (!session->socket) {
        close_websocket(session);
        return false;
    }
    char device_id[kXiaozhiDeviceIdSize] = {};
    char client_id[kXiaozhiClientIdSize] = {};
    xiaozhi_format_device_id(device_id, sizeof(device_id));
    if (!xiaozhi_load_or_create_client_id(client_id, sizeof(client_id))) {
        close_websocket(session);
        return false;
    }
    char headers[192] = {};
    xiaozhi_protocol::format_websocket_headers(headers,
                                               sizeof(headers),
                                               version,
                                               device_id,
                                               client_id);
    esp_transport_ws_set_path(session->socket, path);
    if (!websocket_option_set(
            esp_transport_ws_set_user_agent(session->socket, kOfficialUserAgent),
            "User-Agent") ||
        !websocket_option_set(esp_transport_ws_set_headers(session->socket, headers),
                              "headers")) {
        close_websocket(session);
        return false;
    }
    if (token && token[0] != '\0') {
        char authorization[300] = {};
        xiaozhi_protocol::format_websocket_authorization(authorization,
                                                         sizeof(authorization),
                                                         token);
        if (!websocket_option_set(
                esp_transport_ws_set_auth(session->socket, authorization),
                "Authorization")) {
            close_websocket(session);
            return false;
        }
    }
    int connect_result = esp_transport_connect(session->socket,
                                               host,
                                               port,
                                               kTimeoutMs);
    int upgrade_status = esp_transport_ws_get_upgrade_request_status(session->socket);
    if (connect_result != 0 || upgrade_status != 101) {
        close_websocket(session);
        return false;
    }
    session->version = version > 0 ? version : 1;
    return true;
}

bool parse_server_hello(WebsocketSession *session, const char *json, size_t len)
{
    return session &&
           parse_xiaozhi_server_hello(json,
                                      len,
                                      session->session_id,
                                      sizeof(session->session_id),
                                      &session->output_sample_rate);
}

} // namespace xiaozhi_websocket
