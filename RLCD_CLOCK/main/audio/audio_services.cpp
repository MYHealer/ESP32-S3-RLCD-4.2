// 管理共享 Codec、音频外设和小智全双工音频会话生命周期。
#include "audio_services.h"

#include "app_hardware.h"
#include "app_metadata.h"
#include "app_tick_time.h"
#include "chime_runtime_state.h"
#include "audio_power_lock_ownership.h"
#include "audio_services_internal.h"
#include "atomic_ownership_gate.h"
#include "checked_size.h"
#include "codec_bsp.h"
#include "power_services_internal.h"

#include <atomic>
#include <cstddef>
#include <new>

#include <esp_codec_dev_types.h>
#include <esp_err.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define AUDIO_IDLE_GPIO_CONFIG_FAILED_LOG_FORMAT "audio idle gpio config failed pin=%d err=%s"
#define AUDIO_IDLE_GPIO_LEVEL_FAILED_LOG_FORMAT "audio idle gpio level failed pin=%d err=%s"
#define XIAOZHI_AUDIO_RESIDUAL_CLEANUP_LOG_FORMAT \
    "xiaozhi audio residual cleanup: owner=%d codec=%d lock=%d speaker=%d stream=%d"

namespace {
alignas(CodecPort) unsigned char s_audio_codec_storage[sizeof(CodecPort)] = {};
CodecPort *s_audio_codec = nullptr;
std::atomic<bool> s_audio_codec_present{false};
AtomicOwnershipGate s_audio_playing_gate;
constexpr float kXiaozhiMicGainDb = 37.5f;
constexpr int kXiaozhiAudioSampleRate = 16000;
constexpr size_t kXiaozhiSpeakerFadeSamples = 160;
constexpr size_t kXiaozhiSpeakerTailSilenceSamples = 160;
constexpr uint32_t kXiaozhiWakeFeedbackWarmupMs = 160;
constexpr size_t kXiaozhiWakeFeedbackWarmupChunkSamples = 160;
constexpr uint32_t kXiaozhiSpeakerDrainMs = 30;
constexpr uint32_t kXiaozhiSpeakerCloseRetryMs = 10;
constexpr int kXiaozhiSpeakerCloseAttempts = 2;
constexpr gpio_num_t kAudioMclkGpio = GPIO_NUM_16;
constexpr gpio_num_t kAudioBclkGpio = GPIO_NUM_9;
constexpr gpio_num_t kAudioWsGpio = GPIO_NUM_45;
constexpr gpio_num_t kAudioDinGpio = GPIO_NUM_10;
constexpr gpio_num_t kAudioDoutGpio = GPIO_NUM_8;
constexpr gpio_num_t kAudioPaGpio = GPIO_NUM_46;
constexpr const char *kAudioCodecBoardName = "S3_RLCD_4_2";
constexpr const char *kXiaozhiAudioStartFailedLog = "xiaozhi audio session start failed";
constexpr const char *kXiaozhiAudioBusyLog =
    "xiaozhi audio session busy; another playback still owns the codec";
constexpr const char *kXiaozhiSpeakerCloseFailedLog = "xiaozhi speaker close failed after retry";
constexpr const char *kXiaozhiWakeFeedbackWarmupFailedLog = "xiaozhi wake feedback speaker warmup failed";
constexpr const char *kXiaozhiMicrophoneReadSizeOverflowLog =
    "xiaozhi microphone read size exceeds codec limit";

static_assert(kXiaozhiSpeakerFadeSamples > 0, "xiaozhi speaker fade must be positive");
static_assert(kXiaozhiWakeFeedbackWarmupChunkSamples > 0,
              "xiaozhi wake feedback warmup chunk must be positive");
static_assert(kXiaozhiSpeakerCloseAttempts > 0, "xiaozhi speaker close attempts must be positive");
static_assert(kAudioMclkGpio >= GPIO_NUM_0, "audio MCLK GPIO must be valid");
static_assert(kAudioBclkGpio >= GPIO_NUM_0, "audio BCLK GPIO must be valid");
static_assert(kAudioWsGpio >= GPIO_NUM_0, "audio WS GPIO must be valid");
static_assert(kAudioDinGpio >= GPIO_NUM_0, "audio DIN GPIO must be valid");
static_assert(kAudioDoutGpio >= GPIO_NUM_0, "audio DOUT GPIO must be valid");
static_assert(kAudioPaGpio >= GPIO_NUM_0, "audio PA GPIO must be valid");
} // namespace

extern const uint8_t xiaozhi_popup_pcm_start[] asm("_binary_popup_pcm_start");
extern const uint8_t xiaozhi_popup_pcm_end[] asm("_binary_popup_pcm_end");

static bool s_xiaozhi_speaker_stream_active = false;
static bool s_xiaozhi_speaker_open = false;
static size_t s_xiaozhi_speaker_fade_progress = 0;
static int16_t s_xiaozhi_last_speaker_sample = 0;
static int s_xiaozhi_applied_volume = -1;
static AudioPowerLockOwnership s_audio_power_lock;
static bool s_xiaozhi_audio_session_owned = false;
static void finish_xiaozhi_speaker_stream();

static void reset_xiaozhi_speaker_stream_state()
{
    s_xiaozhi_speaker_stream_active = false;
    s_xiaozhi_speaker_fade_progress = 0;
    s_xiaozhi_last_speaker_sample = 0;
}

static void reset_xiaozhi_speaker_open_state()
{
    s_xiaozhi_speaker_open = false;
    s_xiaozhi_applied_volume = -1;
}

static void reset_xiaozhi_speaker_state()
{
    reset_xiaozhi_speaker_stream_state();
    reset_xiaozhi_speaker_open_state();
}

static bool close_xiaozhi_speaker_with_retry()
{
    if (!s_audio_codec) {
        return false;
    }
    if (s_xiaozhi_speaker_open) {
        vTaskDelay(pdMS_TO_TICKS(kXiaozhiSpeakerDrainMs));
    }
    for (int attempt = 0; attempt < kXiaozhiSpeakerCloseAttempts; ++attempt) {
        if (s_audio_codec->CodecPort_CloseSpeaker()) {
            reset_xiaozhi_speaker_open_state();
            return true;
        }
        if (attempt + 1 < kXiaozhiSpeakerCloseAttempts) {
            vTaskDelay(pdMS_TO_TICKS(kXiaozhiSpeakerCloseRetryMs));
        }
    }
    ESP_LOGW(TAG, "%s", kXiaozhiSpeakerCloseFailedLog);
    return false;
}

static void configure_audio_idle_gpio(gpio_num_t pin, gpio_mode_t mode, gpio_pulldown_t pull_down)
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = mode;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = pull_down;
    cfg.intr_type = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, AUDIO_IDLE_GPIO_CONFIG_FAILED_LOG_FORMAT, (int)pin, esp_err_to_name(err));
        return;
    }
    if (mode == GPIO_MODE_OUTPUT) {
        err = gpio_set_level(pin, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, AUDIO_IDLE_GPIO_LEVEL_FAILED_LOG_FORMAT, (int)pin, esp_err_to_name(err));
        }
    }
}

static void configure_audio_idle_output(gpio_num_t pin)
{
    configure_audio_idle_gpio(pin, GPIO_MODE_OUTPUT, GPIO_PULLDOWN_DISABLE);
}

static void configure_audio_idle_input(gpio_num_t pin)
{
    configure_audio_idle_gpio(pin, GPIO_MODE_INPUT, GPIO_PULLDOWN_ENABLE);
}

void park_unused_audio_peripherals()
{
    configure_audio_idle_output(kAudioPaGpio);
    configure_audio_idle_output(kAudioMclkGpio);
    configure_audio_idle_output(kAudioBclkGpio);
    configure_audio_idle_output(kAudioWsGpio);
    configure_audio_idle_output(kAudioDoutGpio);
    configure_audio_idle_input(kAudioDinGpio);
}

bool audio_try_mark_playing()
{
    return s_audio_playing_gate.try_acquire();
}

void audio_clear_playing()
{
    s_audio_playing_gate.release();
}

bool is_audio_playing()
{
    return s_audio_playing_gate.active();
}

bool wait_for_audio_playback_idle(uint32_t timeout_ms,
                                  uint32_t poll_interval_ms,
                                  AudioStopRequestedCallback stop_requested)
{
    if (!is_audio_playing()) {
        return true;
    }
    if (timeout_ms == 0 || poll_interval_ms == 0) {
        return false;
    }
    for (uint32_t waited_ms = 0;
         is_audio_playing() && waited_ms < timeout_ms &&
             (!stop_requested || !stop_requested());) {
        const uint32_t remaining_ms = timeout_ms - waited_ms;
        const uint32_t delay_ms =
            poll_interval_ms < remaining_ms ? poll_interval_ms : remaining_ms;
        const TickType_t delay_ticks =
            app_tick_nonzero_delay(pdMS_TO_TICKS(delay_ms));
        vTaskDelay(delay_ticks);
        waited_ms += delay_ms;
    }
    return !is_audio_playing();
}

bool audio_codec_active()
{
    return s_audio_codec_present.load(std::memory_order_acquire);
}

static CodecPort *ensure_audio_codec(bool microphone_enabled)
{
    if (!s_audio_codec) {
        CodecPort *codec = new (s_audio_codec_storage) CodecPort(app_i2c(),
                                                                 kAudioCodecBoardName,
                                                                 microphone_enabled);
        if (!codec->CodecPort_IsReady()) {
            ESP_LOGW(TAG, "audio codec playback handle unavailable");
            codec->~CodecPort();
        } else {
            s_audio_codec = codec;
            s_audio_codec_present.store(true, std::memory_order_release);
        }
    }
    return s_audio_codec;
}

static void release_audio_codec()
{
    if (s_audio_codec) {
        s_audio_codec_present.store(false, std::memory_order_release);
        s_audio_codec->~CodecPort();
        s_audio_codec = nullptr;
    }
}

void audio_finish_playback()
{
    release_audio_codec();
    park_unused_audio_peripherals();
    s_audio_power_lock.release();
    s_xiaozhi_audio_session_owned = false;
    audio_clear_playing();
}

static CodecPort *prepare_audio_codec(bool microphone_enabled)
{
    if (!s_audio_power_lock.acquire()) {
        ESP_LOGW(TAG, "audio PM lock unavailable");
        return nullptr;
    }
    CodecPort *codec = ensure_audio_codec(microphone_enabled);
    if (!codec) {
        // 不把失败构造后的 GPIO 和三类 PM 锁留给调用方兜底；播放门仍由
        // 发起方通过统一 audio_finish_playback() 按原所有权顺序归还。
        park_unused_audio_peripherals();
        s_audio_power_lock.release();
    }
    return codec;
}

CodecPort *audio_prepare_codec_for_playback()
{
    return prepare_audio_codec(false);
}

bool start_xiaozhi_audio_session()
{
    if (!audio_try_mark_playing()) {
        ESP_LOGW(TAG, "%s", kXiaozhiAudioBusyLog);
        return false;
    }
    s_xiaozhi_audio_session_owned = true;
    CodecPort *codec = prepare_audio_codec(true);
    if (!codec || !codec->CodecPort_OpenXiaozhiMic()) {
        ESP_LOGW(TAG, "%s", kXiaozhiAudioStartFailedLog);
        audio_finish_playback();
        return false;
    }
    codec->CodecPort_SetMicGain(kXiaozhiMicGainDb);
    reset_xiaozhi_speaker_state();
    return true;
}

void stop_xiaozhi_audio_session()
{
    bool playback_marked = is_audio_playing();
    bool resources_active = s_xiaozhi_audio_session_owned ||
                            s_xiaozhi_speaker_open ||
                            s_xiaozhi_speaker_stream_active ||
                            (!playback_marked &&
                             (s_audio_codec || s_audio_power_lock.active()));
    if (!resources_active) {
        return;
    }
    if (!playback_marked) {
        ESP_LOGW(TAG,
                 XIAOZHI_AUDIO_RESIDUAL_CLEANUP_LOG_FORMAT,
                 s_xiaozhi_audio_session_owned ? 1 : 0,
                 s_audio_codec ? 1 : 0,
                 s_audio_power_lock.active() ? 1 : 0,
                 s_xiaozhi_speaker_open ? 1 : 0,
                 s_xiaozhi_speaker_stream_active ? 1 : 0);
    }
    if (s_audio_codec) {
        finish_xiaozhi_speaker_stream();
        (void)close_xiaozhi_speaker_with_retry();
        s_audio_codec->CodecPort_CloseMic();
    }
    reset_xiaozhi_speaker_state();
    audio_finish_playback();
}

bool set_xiaozhi_audio_high_performance(bool enabled)
{
    // The shared playback gate can be owned by a chime, alarm or setup prompt.
    // Xiaozhi power-state transitions must not change another owner's CPU lock.
    if (s_xiaozhi_audio_session_owned && is_audio_playing()) {
        return set_audio_performance_mode(enabled);
    }
    return true;
}

int read_xiaozhi_microphone(void *buffer, size_t bytes)
{
    if (!s_audio_codec || !buffer || bytes == 0) {
        return ESP_FAIL;
    }
    int codec_bytes = 0;
    if (!app_memory::checked_size_to_int(bytes, &codec_bytes)) {
        ESP_LOGW(TAG, "%s", kXiaozhiMicrophoneReadSizeOverflowLog);
        return ESP_FAIL;
    }
    return s_audio_codec->CodecPort_EchoRead(buffer, codec_bytes);
}

int write_xiaozhi_speaker(const int16_t *mono_samples, size_t sample_count, int sample_rate)
{
    if (!s_audio_codec || !mono_samples || sample_count == 0 ||
        !s_audio_codec->CodecPort_OpenXiaozhiSpeaker(sample_rate)) {
        return ESP_FAIL;
    }
    s_xiaozhi_speaker_open = true;
    apply_xiaozhi_speaker_volume(chime_runtime_volume_percent());
    // 官方同板卡使用标准单声道 TX；RX 的四时隙 TDM 麦克风/参考声道
    // 与播放并行运行，因此这里直接写入服务器提供的 mono PCM。
    constexpr size_t kFramesPerChunk = 160;
    int16_t mono[kFramesPerChunk] = {};
    size_t offset = 0;
    while (offset < sample_count) {
        size_t frames = sample_count - offset;
        if (frames > kFramesPerChunk) {
            frames = kFramesPerChunk;
        }
        for (size_t frame = 0; frame < frames; ++frame) {
            int16_t sample = mono_samples[offset + frame];
            if (s_xiaozhi_speaker_fade_progress < kXiaozhiSpeakerFadeSamples) {
                sample = static_cast<int16_t>(
                    (static_cast<int32_t>(sample) *
                     static_cast<int32_t>(s_xiaozhi_speaker_fade_progress)) /
                    static_cast<int32_t>(kXiaozhiSpeakerFadeSamples));
                ++s_xiaozhi_speaker_fade_progress;
            }
            mono[frame] = sample;
            s_xiaozhi_last_speaker_sample = sample;
            s_xiaozhi_speaker_stream_active = true;
        }
        int written = s_audio_codec->CodecPort_PlayWrite(mono,
                                                         static_cast<int>(frames * sizeof(int16_t)));
        if (written != ESP_CODEC_DEV_OK) {
            return written;
        }
        offset += frames;
    }
    return ESP_CODEC_DEV_OK;
}

void apply_xiaozhi_speaker_volume(int volume_percent)
{
    if (volume_percent < 0) {
        volume_percent = 0;
    } else if (volume_percent > 100) {
        volume_percent = 100;
    }
    if (!s_audio_codec || !s_xiaozhi_speaker_open || s_xiaozhi_applied_volume == volume_percent) {
        return;
    }
    s_audio_codec->CodecPort_SetSpeakerVol(volume_percent);
    s_xiaozhi_applied_volume = volume_percent;
}

static void finish_xiaozhi_speaker_stream()
{
    if (!s_audio_codec || !s_xiaozhi_speaker_stream_active) {
        reset_xiaozhi_speaker_stream_state();
        return;
    }
    int16_t tail[kXiaozhiSpeakerFadeSamples + kXiaozhiSpeakerTailSilenceSamples] = {};
    for (size_t index = 0; index < kXiaozhiSpeakerFadeSamples; ++index) {
        tail[index] = static_cast<int16_t>(
            (static_cast<int32_t>(s_xiaozhi_last_speaker_sample) *
             static_cast<int32_t>(kXiaozhiSpeakerFadeSamples - index - 1)) /
            static_cast<int32_t>(kXiaozhiSpeakerFadeSamples));
    }
    (void)write_xiaozhi_speaker(tail,
                                sizeof(tail) / sizeof(tail[0]),
                                kXiaozhiAudioSampleRate);
    reset_xiaozhi_speaker_stream_state();
}

static bool warm_up_xiaozhi_wake_feedback_speaker()
{
    if (!s_audio_codec ||
        !s_audio_codec->CodecPort_OpenXiaozhiSpeaker(kXiaozhiAudioSampleRate)) {
        return false;
    }
    s_xiaozhi_speaker_open = true;
    s_audio_codec->CodecPort_SetSpeakerVol(0);
    s_xiaozhi_applied_volume = 0;

    int16_t silence[kXiaozhiWakeFeedbackWarmupChunkSamples] = {};
    size_t remaining =
        static_cast<size_t>(kXiaozhiAudioSampleRate) * kXiaozhiWakeFeedbackWarmupMs / 1000U;
    while (remaining > 0) {
        size_t samples = remaining < kXiaozhiWakeFeedbackWarmupChunkSamples
                             ? remaining
                             : kXiaozhiWakeFeedbackWarmupChunkSamples;
        if (s_audio_codec->CodecPort_PlayWrite(silence,
                                               static_cast<int>(samples * sizeof(int16_t))) !=
            ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "%s", kXiaozhiWakeFeedbackWarmupFailedLog);
            return false;
        }
        remaining -= samples;
    }

    reset_xiaozhi_speaker_stream_state();
    return true;
}

bool resume_xiaozhi_microphone_after_playback()
{
    if (!s_audio_codec) {
        return false;
    }
    // STD TX 与 TDM RX 是官方同板卡验证过的全双工布局。结束播放只关闭
    // 扬声器，麦克风/AEC 流保持连续，避免丢失用户插话的开头。
    finish_xiaozhi_speaker_stream();
    bool closed = close_xiaozhi_speaker_with_retry();
    ESP_LOGI(TAG, "xiaozhi duplex microphone kept active: speaker_closed=%d", closed ? 1 : 0);
    return closed;
}

bool play_xiaozhi_wake_feedback()
{
    // This is the upstream 78/xiaozhi-esp32 assets/common/popup.ogg converted
    // to 16 kHz mono PCM so the existing shared codec path can play it without
    // importing the upstream Ogg demuxer or a second audio framework.
    size_t pcm_bytes = static_cast<size_t>(xiaozhi_popup_pcm_end - xiaozhi_popup_pcm_start);
    bool pcm_valid = pcm_bytes > 0 && (pcm_bytes % sizeof(int16_t)) == 0;
    finish_xiaozhi_speaker_stream();
    bool clean_start = close_xiaozhi_speaker_with_retry();
    bool warmed_up = clean_start && warm_up_xiaozhi_wake_feedback_speaker();
    bool played = warmed_up && pcm_valid &&
                  write_xiaozhi_speaker(
                      reinterpret_cast<const int16_t *>(xiaozhi_popup_pcm_start),
                      pcm_bytes / sizeof(int16_t),
                      kXiaozhiAudioSampleRate) == ESP_CODEC_DEV_OK;
    bool restored = resume_xiaozhi_microphone_after_playback();
    ESP_LOGI(TAG,
             "xiaozhi wake feedback: warmup=%d played=%d microphone=%d",
             warmed_up,
             played,
             restored);
    return played && restored;
}

void smooth_xiaozhi_speaker_segment_transition()
{
    // sentence_start messages are serialized between the preceding and next
    // audio packets, so this can safely finish the old waveform without
    // closing the codec or toggling the PA.
    finish_xiaozhi_speaker_stream();
}

void abort_xiaozhi_speaker_playback()
{
    if (!s_audio_codec) {
        return;
    }
    finish_xiaozhi_speaker_stream();
    bool closed = close_xiaozhi_speaker_with_retry();
    ESP_LOGI(TAG, "xiaozhi speaker playback aborted: closed=%d", closed ? 1 : 0);
}
