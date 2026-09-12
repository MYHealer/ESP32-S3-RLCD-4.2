// 声明小智入站对话事件对会话状态和页面快照的统一应用入口。
#pragma once

#include "xiaozhi_websocket_session.h"

#include <cstddef>

namespace xiaozhi_conversation_events {

void clear_tts_timing_state(xiaozhi_websocket::WebsocketSession &session);
bool user_subtitle_hold_active(const xiaozhi_websocket::WebsocketSession *session);
void publish_pending_assistant_text(xiaozhi_websocket::WebsocketSession *session);
void update_incoming_text(xiaozhi_websocket::WebsocketSession *session,
                          const char *json,
                          size_t len);
bool tts_final_frames_settled(const xiaozhi_websocket::WebsocketSession &session);

} // namespace xiaozhi_conversation_events
