// 处理小智 WebSocket 握手及入站文本、MCP 的串行分派。
#include "xiaozhi_conversation_io.h"

#include "app_tick_time.h"
#include "weather_city_mcp.h"
#include "xiaozhi_conversation_events.h"
#include "xiaozhi_mcp.h"
#include "xiaozhi_protocol_utils.h"
#include "xiaozhi_voice.h"

#include <esp_transport.h>
#include <esp_transport_ws.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool xiaozhi_start_voice_protocol_session(
    xiaozhi_websocket::WebsocketSession *session,
    char *buffer,
    size_t buffer_capacity)
{
    if (!session || !buffer || buffer_capacity == 0) {
        return false;
    }
    char hello[192] = {};
    xiaozhi_protocol::format_client_hello(hello,
                                          sizeof(hello),
                                          session->version);
    if (!xiaozhi_websocket::websocket_send_text(session, hello)) {
        return false;
    }
    TickType_t deadline = xTaskGetTickCount() +
                          pdMS_TO_TICKS(xiaozhi_websocket::kTimeoutMs);
    while (app_tick_deadline_pending(xTaskGetTickCount(), deadline) &&
           session->session_id[0] == '\0') {
        int received = esp_transport_read(session->socket,
                                          buffer,
                                          buffer_capacity,
                                          500);
        if (received > 0 &&
            esp_transport_ws_get_read_opcode(session->socket) ==
                WS_TRANSPORT_OPCODES_TEXT) {
            xiaozhi_websocket::parse_server_hello(
                session,
                buffer,
                static_cast<size_t>(received));
        }
    }
    return session->session_id[0] != '\0' &&
           xiaozhi_websocket::websocket_send_listen_start(session);
}

bool xiaozhi_handle_incoming_text_frame(
    xiaozhi_websocket::WebsocketSession *session,
    char *buffer,
    size_t buffer_capacity,
    size_t received)
{
    if (!session || !buffer || buffer_capacity == 0 || received == 0 ||
        received > buffer_capacity) {
        return false;
    }
    bool weather_city_call = xiaozhi_mcp_message_calls_weather_city(
        buffer,
        received);
    bool websocket_lock_suspended = false;
    if (weather_city_call) {
        // WebSocket 整个会话持有公共 HTTP 事务锁。天气城市查询前暂停
        // AEC 并临时让出锁，查询结束后恢复原所有权。
        xiaozhi_voice_pause_streaming();
        websocket_lock_suspended =
            xiaozhi_websocket::suspend_websocket_transaction_lock(session);
    }
    XiaozhiMcpMessageResult mcp_result = xiaozhi_mcp_handle_message(
        buffer,
        received,
        session->session_id,
        buffer,
        buffer_capacity,
        !session->exit_after_reply_requested);
    if (weather_city_call) {
        bool lock_restored =
            xiaozhi_websocket::restore_websocket_transaction_lock(
                session,
                websocket_lock_suspended);
        xiaozhi_voice_set_streaming(true);
        if (!lock_restored) {
            return false;
        }
    }
    if (mcp_result == kXiaozhiMcpHandledWithResponse) {
        return xiaozhi_websocket::websocket_send_text(session, buffer);
    }
    if (mcp_result == kXiaozhiMcpNotHandled) {
        xiaozhi_conversation_events::update_incoming_text(session,
                                                          buffer,
                                                          received);
    }
    return true;
}
