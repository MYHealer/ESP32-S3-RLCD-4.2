// 声明仅供网络检测执行器写入状态与逐行结果的内部接口。
#pragma once

#include "network_diagnostics_state.h"

#include <stddef.h>

bool network_diagnostics_state_init();
void network_diag_state_clear(NetworkDiagState state);
bool network_diag_state_begin(const char *const *line_formats,
                              size_t format_count,
                              const char *status_text);
bool network_diag_state_publish(const char *const *lines,
                                size_t line_count,
                                NetworkDiagState state);
void network_diag_state_store(NetworkDiagState state);
void network_diag_line_store(int index, const char *text);
