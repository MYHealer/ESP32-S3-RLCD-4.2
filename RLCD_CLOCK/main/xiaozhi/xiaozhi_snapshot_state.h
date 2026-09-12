// 管理小智页面快照的互斥存储、状态更新和 UI 通知。
#pragma once

#include "xiaozhi_ai.h"

inline constexpr const char *kXiaozhiDefaultStatus = "小智准备中";

bool xiaozhi_snapshot_state_init();
void xiaozhi_snapshot_state_deinit();

void xiaozhi_snapshot_set(XiaozhiAiState state,
                          const char *status,
                          const char *detail,
                          const char *binding_code = nullptr);
void xiaozhi_snapshot_set_status_preserving_detail(XiaozhiAiState state,
                                                    const char *status);
void xiaozhi_snapshot_set_emotion(const char *emotion);
void xiaozhi_snapshot_mark_user_activity();
void xiaozhi_snapshot_get(XiaozhiAiSnapshot *out);
bool xiaozhi_snapshot_get_if_changed(uint32_t *version,
                                     XiaozhiAiSnapshot *out);
XiaozhiActivitySnapshot xiaozhi_snapshot_activity_load();
