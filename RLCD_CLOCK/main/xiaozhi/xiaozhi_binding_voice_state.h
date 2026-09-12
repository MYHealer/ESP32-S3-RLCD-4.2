// 管理小智绑定码播报任务使用的线程安全固定状态。
#pragma once

#include <stddef.h>

inline constexpr size_t kXiaozhiBindingCodeStorageSize = 24;

bool xiaozhi_binding_voice_state_init();
void xiaozhi_binding_voice_state_deinit();
bool xiaozhi_binding_voice_needs_announcement(const char *binding_code);
bool xiaozhi_binding_voice_record_announced(const char *binding_code);
bool xiaozhi_binding_voice_store_pending(const char *binding_code);
bool xiaozhi_binding_voice_take_pending(char *out, size_t out_len);
