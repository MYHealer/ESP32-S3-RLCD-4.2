// 验证小智编解码运行时成功持有、失败回滚和重复初始化资源语义。
#include "xiaozhi_voice_codec.h"

#include "esp_opus_dec.h"
#include "esp_opus_enc.h"

#include <assert.h>
#include <cstring>
#include <initializer_list>

namespace {
enum class FailurePoint {
    None,
    Encoder,
    Decoder,
    RateConverter,
    FrameSize,
};

FailurePoint s_failure = FailurePoint::None;
int s_encoder_open_count = 0;
int s_encoder_close_count = 0;
int s_decoder_open_count = 0;
int s_decoder_close_count = 0;
int s_rate_open_count = 0;
int s_rate_close_count = 0;
int s_encoder_handle = 0;
int s_decoder_handle = 0;
int s_rate_handle = 0;

void reset_fakes(FailurePoint failure = FailurePoint::None)
{
    s_failure = failure;
    s_encoder_open_count = 0;
    s_encoder_close_count = 0;
    s_decoder_open_count = 0;
    s_decoder_close_count = 0;
    s_rate_open_count = 0;
    s_rate_close_count = 0;
}

void assert_runtime_empty(const VoiceCodecRuntime &runtime)
{
    assert(runtime.encoder == nullptr);
    assert(runtime.decoder == nullptr);
    assert(runtime.rate_converter == nullptr);
    assert(runtime.encode_buffers == nullptr);
    assert(runtime.encoder_input_size == 0);
    assert(runtime.encoder_output_size == 0);
}

void assert_buffers_zero(const VoiceEncodeBuffers &buffers)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(&buffers);
    for (size_t index = 0; index < sizeof(buffers); ++index) {
        assert(bytes[index] == 0);
    }
}
} // namespace

extern "C" esp_audio_err_t esp_opus_enc_open(void *, uint32_t, void **handle)
{
    ++s_encoder_open_count;
    *handle = s_failure == FailurePoint::Encoder ? nullptr : &s_encoder_handle;
    return *handle ? ESP_AUDIO_ERR_OK : -1;
}

extern "C" esp_audio_err_t esp_opus_dec_open(void *, uint32_t, void **handle)
{
    ++s_decoder_open_count;
    *handle = s_failure == FailurePoint::Decoder ? nullptr : &s_decoder_handle;
    return *handle ? ESP_AUDIO_ERR_OK : -1;
}

extern "C" esp_ae_err_t esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t *,
                                               esp_ae_rate_cvt_handle_t *handle)
{
    ++s_rate_open_count;
    *handle = s_failure == FailurePoint::RateConverter ? nullptr : &s_rate_handle;
    return *handle ? ESP_AE_ERR_OK : -1;
}

extern "C" esp_audio_err_t esp_opus_enc_get_frame_size(void *, int *input_size, int *output_size)
{
    if (s_failure == FailurePoint::FrameSize) {
        return -1;
    }
    *input_size = static_cast<int>(sizeof(VoiceEncodeBuffers::mono));
    *output_size = static_cast<int>(sizeof(VoiceEncodeBuffers::opus));
    return ESP_AUDIO_ERR_OK;
}

extern "C" void esp_opus_enc_close(void *)
{
    ++s_encoder_close_count;
}

extern "C" esp_audio_err_t esp_opus_dec_close(void *)
{
    ++s_decoder_close_count;
    return ESP_AUDIO_ERR_OK;
}

extern "C" void esp_ae_rate_cvt_close(esp_ae_rate_cvt_handle_t)
{
    ++s_rate_close_count;
}

int main()
{
    {
        reset_fakes();
        VoiceEncodeBuffers buffers;
        std::memset(&buffers, 0xa5, sizeof(buffers));
        VoiceCodecRuntime runtime;
        assert(runtime.initialize(16000, &buffers));
        assert(runtime.encoder != nullptr);
        assert(runtime.decoder != nullptr);
        assert(runtime.rate_converter == nullptr);
        assert(runtime.encode_buffers == &buffers);
        assert_buffers_zero(buffers);
        assert(runtime.encoder_input_size == static_cast<int>(sizeof(VoiceEncodeBuffers::mono)));
        assert(runtime.encoder_output_size == static_cast<int>(sizeof(VoiceEncodeBuffers::opus)));
        runtime.release();
        assert_runtime_empty(runtime);
        assert(s_encoder_close_count == 1);
        assert(s_decoder_close_count == 1);
        assert(s_rate_close_count == 0);
    }

    {
        reset_fakes();
        VoiceEncodeBuffers buffers = {};
        VoiceCodecRuntime runtime;
        assert(runtime.initialize(24000, &buffers));
        assert(runtime.rate_converter != nullptr);
        assert(runtime.initialize(16000, &buffers));
        assert(runtime.rate_converter == nullptr);
        assert(s_encoder_open_count == 2);
        assert(s_encoder_close_count == 1);
        assert(s_decoder_open_count == 2);
        assert(s_decoder_close_count == 1);
        assert(s_rate_open_count == 1);
        assert(s_rate_close_count == 1);
    }

    for (FailurePoint failure : {FailurePoint::Encoder,
                                 FailurePoint::Decoder,
                                 FailurePoint::RateConverter,
                                 FailurePoint::FrameSize}) {
        reset_fakes(failure);
        VoiceEncodeBuffers buffers = {};
        VoiceCodecRuntime runtime;
        const int output_rate = failure == FailurePoint::RateConverter ? 24000 : 16000;
        assert(!runtime.initialize(output_rate, &buffers));
        assert_runtime_empty(runtime);
        assert(s_encoder_open_count == 1);
        assert(s_decoder_open_count == (failure == FailurePoint::Encoder ? 0 : 1));
        assert(s_rate_open_count == (failure == FailurePoint::RateConverter ? 1 : 0));
        assert(s_encoder_close_count == (failure == FailurePoint::Encoder ? 0 : 1));
        assert(s_decoder_close_count == (failure == FailurePoint::Encoder ||
                                         failure == FailurePoint::Decoder ? 0 : 1));
        assert(s_rate_close_count == 0);
    }

    {
        reset_fakes();
        VoiceCodecRuntime runtime;
        assert(!runtime.initialize(24000, nullptr));
        assert_runtime_empty(runtime);
        assert(s_rate_open_count == 1);
        assert(s_encoder_close_count == 1);
        assert(s_decoder_close_count == 1);
        assert(s_rate_close_count == 1);
    }

    return 0;
}
