// 应用小智 STT、TTS 和情绪事件，集中维护字幕与播放尾帧状态。
#include "xiaozhi_conversation_events.h"

#include "app_tick_time.h"
#include "xiaozhi_conversation_policy.h"
#include "xiaozhi_incoming_event_parser.h"
#include "xiaozhi_snapshot_state.h"
#include "xiaozhi_tts_playback.h"
#include "xiaozhi_voice.h"

#include <esp_log.h>
#include <freertos/task.h>
#include <string.h>

namespace xiaozhi_conversation_events {
namespace {

constexpr uint32_t kExitReplyTimeoutMs = 15000;
constexpr uint32_t kUserSubtitleMinVisibleMs = 2200;
constexpr const char *kTag = "WeatherClock";
constexpr const char *kSpeakingStatus = "小智正在说话";

void handle_incoming_tts_start(xiaozhi_websocket::WebsocketSession &session)
{
    // The device currently does not support reliable playback interruption.
    // Stop uploading microphone/AEC output while the speaker is active so its
    // echo cannot be mistaken by the server for a new user turn.
    xiaozhi_voice_pause_streaming();
    session.server_speaking = true;
    session.exit_reply_started = session.exit_after_reply_requested;
    session.resume_listening_pending = false;
    session.discard_tts_audio = false;
    session.empty_reply_continuation_pending = false;
    session.tts_started_tick = xTaskGetTickCount();
    session.tts_started_tick_set = true;
    session.tts_stop_received_tick = 0;
    session.last_tts_audio_tick = 0;
    ESP_LOGI(kTag, "Xiaozhi TTS started");
    if (user_subtitle_hold_active(&session)) {
        xiaozhi_snapshot_set_status_preserving_detail(kXiaozhiAiSpeaking,
                                                      kSpeakingStatus);
    } else {
        xiaozhi_snapshot_set(kXiaozhiAiSpeaking,
                             kSpeakingStatus,
                             "直接说话即可打断");
    }
}

void handle_incoming_tts_stop(xiaozhi_websocket::WebsocketSession &session)
{
    // Keep the speaker open until any final binary frames already in
    // the WebSocket have been drained. The duplex microphone/AEC
    // stream continues uninterrupted throughout this transition.
    session.server_speaking = true;
    session.resume_listening_pending = true;
    session.tts_stop_received_tick = xTaskGetTickCount();
    XiaozhiTtsPlaybackSnapshot playback = {};
    xiaozhi_tts_playback_get_snapshot(&playback);
    ESP_LOGI(kTag,
             "Xiaozhi TTS stop received: queued=%u busy=%d",
             static_cast<unsigned>(playback.queued_bytes),
             playback.busy ? 1 : 0);
}

void handle_incoming_tts_sentence_start(xiaozhi_websocket::WebsocketSession &session,
                                        const char *text)
{
    if (session.resume_listening_pending) {
        TickType_t now = xTaskGetTickCount();
        uint32_t continuation_delay_ms = static_cast<uint32_t>(
            (static_cast<uint64_t>(now - session.tts_stop_received_tick) *
             1000U) /
            configTICK_RATE_HZ);
        ESP_LOGI(kTag,
                 "Xiaozhi TTS continuation after stop: delay=%u ms",
                 static_cast<unsigned>(continuation_delay_ms));
    }
    session.server_speaking = true;
    session.resume_listening_pending = false;
    session.tts_stop_received_tick = 0;
    session.turn_assistant_text_received = true;
    session.empty_reply_continuation_pending = false;
    session.exit_reply_started = session.exit_after_reply_requested;
    if (!session.tts_started_tick_set) {
        session.tts_started_tick = xTaskGetTickCount();
        session.tts_started_tick_set = true;
    }
    ESP_LOGI(kTag,
             "Xiaozhi assistant text (%u bytes): %.160s",
             static_cast<unsigned>(strlen(text)),
             text);
    // The service can emit tool progress markers such as
    // "% get_weather..." as sentence events. They are not spoken
    // subtitles and look like corrupted text on the compact page.
    if (strncmp(text, "% ", 2) == 0) {
        return;
    }
    if (user_subtitle_hold_active(&session)) {
        strlcpy(session.pending_assistant_text,
                text,
                sizeof(session.pending_assistant_text));
    } else {
        xiaozhi_snapshot_set(kXiaozhiAiSpeaking, kSpeakingStatus, text);
    }
}

void handle_incoming_stt(xiaozhi_websocket::WebsocketSession &session,
                         const char *text)
{
    ESP_LOGI(kTag,
             "Xiaozhi user text (%u bytes): %.160s",
             static_cast<unsigned>(strlen(text)),
             text);
    session.user_text_hold_until =
        xTaskGetTickCount() + pdMS_TO_TICKS(kUserSubtitleMinVisibleMs);
    session.user_text_hold_set = true;
    session.pending_assistant_text[0] = '\0';
    session.turn_user_text_received = true;
    session.turn_assistant_text_received = false;
    session.turn_assistant_audio_received = false;
    session.empty_reply_continuation_pending = false;
    xiaozhi_snapshot_set(kXiaozhiAiListening, "正在对话", text);
    if (xiaozhi_user_requested_exit(text)) {
        session.exit_after_reply_requested = true;
        session.exit_reply_started = false;
        session.exit_reply_deadline =
            xTaskGetTickCount() + pdMS_TO_TICKS(kExitReplyTimeoutMs);
        session.exit_reply_deadline_set = true;
        ESP_LOGI(kTag, "Xiaozhi voice exit requested, waiting for farewell");
    }
}

void handle_incoming_llm_emotion(const char *emotion)
{
    if (!emotion) {
        return;
    }
    ESP_LOGI(kTag, "Xiaozhi emotion: %.23s", emotion);
    xiaozhi_snapshot_set_emotion(emotion);
}

} // namespace

void clear_tts_timing_state(xiaozhi_websocket::WebsocketSession &session)
{
    session.tts_started_tick = 0;
    session.tts_started_tick_set = false;
    session.tts_stop_received_tick = 0;
    session.last_tts_audio_tick = 0;
}

bool user_subtitle_hold_active(const xiaozhi_websocket::WebsocketSession *session)
{
    if (!session || !session->user_text_hold_set) {
        return false;
    }
    return app_tick_deadline_pending(xTaskGetTickCount(),
                                     session->user_text_hold_until);
}

void publish_pending_assistant_text(xiaozhi_websocket::WebsocketSession *session)
{
    if (!session || user_subtitle_hold_active(session)) {
        return;
    }
    session->user_text_hold_set = false;
    session->user_text_hold_until = 0;
    if (session->pending_assistant_text[0] == '\0') {
        return;
    }
    char text[sizeof(session->pending_assistant_text)] = {};
    strlcpy(text, session->pending_assistant_text, sizeof(text));
    session->pending_assistant_text[0] = '\0';
    xiaozhi_snapshot_set(kXiaozhiAiSpeaking, kSpeakingStatus, text);
}

void update_incoming_text(xiaozhi_websocket::WebsocketSession *session,
                          const char *json,
                          size_t len)
{
    XiaozhiIncomingEvent event;
    if (!event.parse(json, len) || !session) {
        return;
    }
    switch (event.type()) {
        case XiaozhiIncomingEventType::kTtsStart:
            handle_incoming_tts_start(*session);
            break;

        case XiaozhiIncomingEventType::kTtsStop:
            handle_incoming_tts_stop(*session);
            break;

        case XiaozhiIncomingEventType::kTtsSentenceStart:
            handle_incoming_tts_sentence_start(*session, event.text());
            break;

        case XiaozhiIncomingEventType::kStt:
            handle_incoming_stt(*session, event.text());
            break;

        case XiaozhiIncomingEventType::kLlm:
            handle_incoming_llm_emotion(event.emotion());
            break;

        case XiaozhiIncomingEventType::kUnknown:
            break;
    }
}

bool tts_final_frames_settled(const xiaozhi_websocket::WebsocketSession &session)
{
    return xiaozhi_tts_final_frames_settled(
        static_cast<uint32_t>(xTaskGetTickCount()),
        static_cast<uint32_t>(session.tts_stop_received_tick),
        static_cast<uint32_t>(session.last_tts_audio_tick),
        static_cast<uint32_t>(pdMS_TO_TICKS(kXiaozhiTtsFinalFrameGraceMs)),
        static_cast<uint32_t>(pdMS_TO_TICKS(kXiaozhiTtsPlaybackTailSettleMs)));
}

} // namespace xiaozhi_conversation_events
