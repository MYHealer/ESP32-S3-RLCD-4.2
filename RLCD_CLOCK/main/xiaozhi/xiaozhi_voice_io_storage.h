// 声明小智单会话 WebSocket 输入和音频解码临时区的可复用 PSRAM 所有权。
#pragma once

#include "xiaozhi_audio_frames.h"

#include <cstddef>

constexpr size_t kXiaozhiIncomingAudioBufferSize = 4096;
static_assert(kXiaozhiIncomingAudioBufferSize > 0,
              "Xiaozhi incoming audio buffer must not be empty");

struct XiaozhiVoiceIoBuffers {
    char incoming[kXiaozhiIncomingAudioBufferSize] = {};
    XiaozhiAudioDecodeBuffers audio;
    VoiceEncodeBuffers encode;
};

class XiaozhiVoiceIoLease {
public:
    XiaozhiVoiceIoLease();
    ~XiaozhiVoiceIoLease();

    XiaozhiVoiceIoLease(const XiaozhiVoiceIoLease &) = delete;
    XiaozhiVoiceIoLease &operator=(const XiaozhiVoiceIoLease &) = delete;

    XiaozhiVoiceIoBuffers *get() const
    {
        return buffers_;
    }

    explicit operator bool() const
    {
        return buffers_ != nullptr;
    }

    void reset();

private:
    XiaozhiVoiceIoBuffers *buffers_ = nullptr;
};
