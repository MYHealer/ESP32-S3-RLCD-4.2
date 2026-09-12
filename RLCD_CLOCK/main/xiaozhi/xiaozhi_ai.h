// 管理小智官方激活、会话状态和页面生命周期，不接管本机配网或 OTA。
#pragma once

#include <stddef.h>
#include <stdint.h>

enum XiaozhiAiState {
    kXiaozhiAiInactive = 0,
    kXiaozhiAiWaitingForWifi,
    kXiaozhiAiActivating,
    kXiaozhiAiBinding,
    kXiaozhiAiReady,
    kXiaozhiAiListening,
    kXiaozhiAiSpeaking,
    kXiaozhiAiError,
};

struct XiaozhiAiSnapshot {
    XiaozhiAiState state;
    char status[32];
    char detail[192];
    char binding_code[24];
    char emotion[24];
    uint8_t waveform_level;
    uint32_t activity_sequence;
};

struct XiaozhiActivitySnapshot {
    XiaozhiAiState state;
    uint32_t activity_sequence;
};

void xiaozhi_ai_init();
void xiaozhi_ai_set_page_active(bool active);
void xiaozhi_ai_notify_network_configuration_changed();
bool xiaozhi_ai_page_active();
bool xiaozhi_ai_network_keepalive_active();
void xiaozhi_ai_set_alarm_suspended(bool suspended);
void xiaozhi_ai_set_pomodoro_audio_suspended(bool suspended);
void xiaozhi_ai_get_snapshot(XiaozhiAiSnapshot *out);
bool xiaozhi_ai_get_snapshot_if_changed(uint32_t *version,
                                        XiaozhiAiSnapshot *out);
XiaozhiActivitySnapshot xiaozhi_ai_activity_snapshot_load();
void xiaozhi_ai_clear_activation();
