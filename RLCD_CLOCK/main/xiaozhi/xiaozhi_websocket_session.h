// 声明小智 WebSocket transport 所有权、会话状态和内部操作入口。
#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_transport.h"
#include "freertos/FreeRTOS.h"

namespace xiaozhi_websocket {

constexpr uint32_t kTimeoutMs = 12000;

struct WebsocketSession {
    esp_transport_handle_t parent = nullptr;
    esp_transport_handle_t socket = nullptr;
    int version = 1;
    int output_sample_rate = 16000;
    char session_id[48] = {};
    bool network_transaction_locked = false;
    bool playback_format_logged = false;
    bool server_speaking = false;
    bool resume_listening_pending = false;
    bool discard_tts_audio = false;
    bool exit_after_reply_requested = false;
    bool exit_reply_started = false;
    bool exit_reply_deadline_set = false;
    bool user_text_hold_set = false;
    bool peer_disconnected = false;
    bool tts_started_tick_set = false;
    bool turn_user_text_received = false;
    bool turn_assistant_text_received = false;
    bool turn_assistant_audio_received = false;
    bool empty_reply_continuation_pending = false;
    TickType_t tts_stop_received_tick = 0;
    TickType_t last_tts_audio_tick = 0;
    TickType_t tts_started_tick = 0;
    TickType_t exit_reply_deadline = 0;
    TickType_t user_text_hold_until = 0;
    TickType_t empty_reply_continuation_deadline = 0;
    char pending_assistant_text[192] = {};
};

void close_websocket(WebsocketSession *session);

class ScopedWebsocketSession {
public:
    ScopedWebsocketSession() = default;
    ~ScopedWebsocketSession();

    ScopedWebsocketSession(const ScopedWebsocketSession &) = delete;
    ScopedWebsocketSession &operator=(const ScopedWebsocketSession &) = delete;

    WebsocketSession &get()
    {
        return session_;
    }

    void reset();

private:
    WebsocketSession session_;
};

bool websocket_send_text(WebsocketSession *session, const char *text);
bool websocket_send_listen_start(WebsocketSession *session);
bool websocket_send_wake_abort(WebsocketSession *session);
bool suspend_websocket_transaction_lock(WebsocketSession *session);
bool restore_websocket_transaction_lock(WebsocketSession *session, bool was_locked);
bool open_websocket(WebsocketSession *session, const char *url, const char *token, int version);
bool parse_server_hello(WebsocketSession *session, const char *json, size_t len);

} // namespace xiaozhi_websocket
