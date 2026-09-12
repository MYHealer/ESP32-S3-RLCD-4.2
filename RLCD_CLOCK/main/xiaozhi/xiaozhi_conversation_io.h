// 声明小智 WebSocket 握手与入站文本/MCP 分派入口。
#pragma once

#include "xiaozhi_websocket_session.h"

#include <cstddef>

bool xiaozhi_start_voice_protocol_session(
    xiaozhi_websocket::WebsocketSession *session,
    char *buffer,
    size_t buffer_capacity);

bool xiaozhi_handle_incoming_text_frame(
    xiaozhi_websocket::WebsocketSession *session,
    char *buffer,
    size_t buffer_capacity,
    size_t received);
