// 声明音频播放、提示音选择和音频电源管理接口。
#pragma once

#include <cstddef>
#include <cstdint>

using AudioStopRequestedCallback = bool (*)();

void hourly_chime_task(void *arg);
void setup_prompt_task(void *);
bool start_chime_playback(int source_slot);
bool play_chime_sound_blocking(int source_slot,
                               AudioStopRequestedCallback stop_requested);
bool play_chime_sound_repeated_blocking(int source_slot,
                                        int repeat_count,
                                        AudioStopRequestedCallback stop_requested);
bool start_setup_prompt_playback();
bool setup_prompt_playback_pending();
bool is_audio_playing();
bool wait_for_audio_playback_idle(uint32_t timeout_ms,
                                  uint32_t poll_interval_ms,
                                  AudioStopRequestedCallback stop_requested);
bool audio_codec_active();
void request_setup_prompt_once();
void request_settings_confirmation_chime();
void play_hourly_chime(int hour, bool enforce_quiet_hours = true);

// 小智页独占使用现有 CodecPort；调用者只能在成功开始会话后读写 PCM，
// 结束时必须调用 stop_xiaozhi_audio_session() 归还音频与轻睡眠锁。
bool start_xiaozhi_audio_session();
void stop_xiaozhi_audio_session();
bool set_xiaozhi_audio_high_performance(bool enabled);
int read_xiaozhi_microphone(void *buffer, size_t bytes);
int write_xiaozhi_speaker(const int16_t *mono_samples, size_t sample_count, int sample_rate);
void apply_xiaozhi_speaker_volume(int volume_percent);
bool resume_xiaozhi_microphone_after_playback();
bool play_xiaozhi_wake_feedback();
void smooth_xiaozhi_speaker_segment_transition();
void abort_xiaozhi_speaker_playback();

// MiPlay 投屏音频会话：独立于小智会话，使用立体声输出。
bool start_miplay_audio_session();
void stop_miplay_audio_session();
int write_stereo_speaker(const int16_t *stereo_samples, size_t frame_count, int sample_rate);
// 直接设置 codec 音量（不检查会话状态）。
void apply_codec_volume_direct(int volume_percent);
