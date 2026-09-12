// 声明小智 WebSocket 服务端 hello 消息的纯 JSON 解析入口。
#pragma once

#include <stddef.h>

bool parse_xiaozhi_server_hello(const char *json,
                                size_t json_len,
                                char *session_id,
                                size_t session_id_len,
                                int *output_sample_rate);
