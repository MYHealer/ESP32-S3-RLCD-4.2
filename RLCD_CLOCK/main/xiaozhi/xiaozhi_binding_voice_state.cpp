// 使用静态任务互斥保护最近已播报和待播的绑定码。
#include "xiaozhi_binding_voice_state.h"

#include "scoped_semaphore_lock.h"
#include "xiaozhi_binding_voice.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string.h>

namespace {
StaticSemaphore_t s_binding_code_mutex_storage = {};
SemaphoreHandle_t s_binding_code_mutex = nullptr;
char s_last_announced_binding_code[kXiaozhiBindingCodeStorageSize] = {};
char s_pending_binding_code[kXiaozhiBindingCodeStorageSize] = {};

void prepare_binding_code(char *out, const char *binding_code)
{
    memset(out, 0, kXiaozhiBindingCodeStorageSize);
    if (!binding_code) {
        return;
    }
    const size_t length = strnlen(binding_code,
                                  kXiaozhiBindingCodeStorageSize - 1);
    memcpy(out, binding_code, length);
}
} // namespace

bool xiaozhi_binding_voice_state_init()
{
    if (s_binding_code_mutex) {
        return true;
    }
    s_binding_code_mutex =
        xSemaphoreCreateMutexStatic(&s_binding_code_mutex_storage);
    return s_binding_code_mutex != nullptr;
}

void xiaozhi_binding_voice_state_deinit()
{
    if (!s_binding_code_mutex) {
        return;
    }
    {
        ScopedSemaphoreLock lock(s_binding_code_mutex);
        if (!lock) {
            return;
        }
        memset(s_last_announced_binding_code, 0, sizeof(s_last_announced_binding_code));
        memset(s_pending_binding_code, 0, sizeof(s_pending_binding_code));
    }
    vSemaphoreDelete(s_binding_code_mutex);
    s_binding_code_mutex = nullptr;
}

bool xiaozhi_binding_voice_needs_announcement(const char *binding_code)
{
    if (!binding_code || binding_code[0] == '\0') {
        return false;
    }
    char last_announced[kXiaozhiBindingCodeStorageSize] = {};
    {
        ScopedSemaphoreLock lock(s_binding_code_mutex);
        if (!lock) {
            return false;
        }
        memcpy(last_announced,
               s_last_announced_binding_code,
               sizeof(last_announced));
    }
    return xiaozhi_binding_voice::should_announce(binding_code,
                                                  last_announced);
}

bool xiaozhi_binding_voice_record_announced(const char *binding_code)
{
    char replacement[kXiaozhiBindingCodeStorageSize] = {};
    prepare_binding_code(replacement, binding_code);
    ScopedSemaphoreLock lock(s_binding_code_mutex);
    if (!lock) {
        return false;
    }
    memcpy(s_last_announced_binding_code,
           replacement,
           sizeof(s_last_announced_binding_code));
    return true;
}

bool xiaozhi_binding_voice_store_pending(const char *binding_code)
{
    char replacement[kXiaozhiBindingCodeStorageSize] = {};
    prepare_binding_code(replacement, binding_code);
    ScopedSemaphoreLock lock(s_binding_code_mutex);
    if (!lock) {
        return false;
    }
    memcpy(s_pending_binding_code,
           replacement,
           sizeof(s_pending_binding_code));
    return true;
}

bool xiaozhi_binding_voice_take_pending(char *out, size_t out_len)
{
    if (!out || out_len < kXiaozhiBindingCodeStorageSize) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return false;
    }
    ScopedSemaphoreLock lock(s_binding_code_mutex);
    if (!lock) {
        out[0] = '\0';
        return false;
    }
    memcpy(out, s_pending_binding_code, sizeof(s_pending_binding_code));
    memset(s_pending_binding_code, 0, sizeof(s_pending_binding_code));
    return out[0] != '\0';
}
