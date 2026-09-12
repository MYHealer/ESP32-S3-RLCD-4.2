// 声明小智激活配置、稳定 Client ID 和 NVS 清理入口。
#pragma once

#include <stddef.h>
#include <stdint.h>

struct cJSON;

inline constexpr size_t kXiaozhiClientIdSize = 37;
inline constexpr size_t kXiaozhiWebsocketConfigFieldSize = 256;

bool xiaozhi_load_websocket_config(char *url,
                                    size_t url_len,
                                    char *token,
                                    size_t token_len,
                                    int32_t *version);
bool xiaozhi_save_activation_config(cJSON *websocket, const char *challenge);
bool xiaozhi_load_or_create_client_id(char *out, size_t out_len);
bool xiaozhi_clear_activation_storage();
