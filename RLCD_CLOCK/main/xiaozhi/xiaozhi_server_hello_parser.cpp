// 解析小智服务端 hello 的会话 ID 和受支持的输出采样率。
#include "xiaozhi_server_hello_parser.h"

#include "xiaozhi_json_owner.h"

#include <string.h>

namespace {
constexpr int kSupportedOutputSampleRate16K = 16000;
constexpr int kSupportedOutputSampleRate24K = 24000;
} // namespace

bool parse_xiaozhi_server_hello(const char *json,
                                size_t json_len,
                                char *session_id,
                                size_t session_id_len,
                                int *output_sample_rate)
{
    if (!json || !session_id || session_id_len == 0 || !output_sample_rate) {
        return false;
    }
    XiaozhiJsonOwner root{cJSON_ParseWithLength(json, json_len)};
    if (!root) {
        return false;
    }
    const cJSON *type = cJSON_GetObjectItem(root.get(), "type");
    const cJSON *transport = cJSON_GetObjectItem(root.get(), "transport");
    const cJSON *server_session_id = cJSON_GetObjectItem(root.get(), "session_id");
    const cJSON *audio_params = cJSON_GetObjectItem(root.get(), "audio_params");
    const bool hello = cJSON_IsString(type) && strcmp(type->valuestring, "hello") == 0 &&
                       cJSON_IsString(transport) && strcmp(transport->valuestring, "websocket") == 0;
    if (hello && cJSON_IsString(server_session_id)) {
        strlcpy(session_id, server_session_id->valuestring, session_id_len);
    }
    if (hello && cJSON_IsObject(audio_params)) {
        const cJSON *rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(rate) &&
            (rate->valueint == kSupportedOutputSampleRate16K ||
             rate->valueint == kSupportedOutputSampleRate24K)) {
            *output_sample_rate = rate->valueint;
        }
    }
    return hello && session_id[0] != '\0';
}
