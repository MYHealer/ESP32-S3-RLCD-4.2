// 管理小智单会话 WebSocket 输入和音频解码临时区的复用与清零。
#include "xiaozhi_voice_io_storage.h"

#include <esp_heap_caps.h>

#include <atomic>
#include <cstring>

namespace {
std::atomic<bool> s_voice_io_in_use{false};
XiaozhiVoiceIoBuffers *s_voice_io_buffers = nullptr;
} // namespace

XiaozhiVoiceIoLease::XiaozhiVoiceIoLease()
{
    bool expected = false;
    if (!s_voice_io_in_use.compare_exchange_strong(expected,
                                                   true,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
        return;
    }

    if (!s_voice_io_buffers) {
        s_voice_io_buffers = static_cast<XiaozhiVoiceIoBuffers *>(heap_caps_calloc(
            1, sizeof(XiaozhiVoiceIoBuffers), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!s_voice_io_buffers) {
        s_voice_io_in_use.store(false, std::memory_order_release);
        return;
    }

    std::memset(s_voice_io_buffers, 0, sizeof(XiaozhiVoiceIoBuffers));
    buffers_ = s_voice_io_buffers;
}

XiaozhiVoiceIoLease::~XiaozhiVoiceIoLease()
{
    reset();
}

void XiaozhiVoiceIoLease::reset()
{
    if (!buffers_) {
        return;
    }
    std::memset(buffers_, 0, sizeof(XiaozhiVoiceIoBuffers));
    buffers_ = nullptr;
    s_voice_io_in_use.store(false, std::memory_order_release);
}
