// 验证小智服务端 hello 的协议字段、采样率和原值保留语义。
#include "xiaozhi_server_hello_parser.h"

#include <assert.h>
#include <string.h>

int main()
{
    char session_id[48] = "unchanged";
    int output_sample_rate = 16000;
    assert(!parse_xiaozhi_server_hello(nullptr,
                                       0,
                                       session_id,
                                       sizeof(session_id),
                                       &output_sample_rate));
    assert(!parse_xiaozhi_server_hello("{}",
                                       2,
                                       nullptr,
                                       0,
                                       &output_sample_rate));
    assert(!parse_xiaozhi_server_hello("{",
                                       1,
                                       session_id,
                                       sizeof(session_id),
                                       &output_sample_rate));
    assert(strcmp(session_id, "unchanged") == 0);
    assert(output_sample_rate == 16000);

    const char wrong_transport[] =
        "{\"type\":\"hello\",\"transport\":\"mqtt\",\"session_id\":\"wrong\"," 
        "\"audio_params\":{\"sample_rate\":24000}}";
    assert(!parse_xiaozhi_server_hello(wrong_transport,
                                       strlen(wrong_transport),
                                       session_id,
                                       sizeof(session_id),
                                       &output_sample_rate));
    assert(strcmp(session_id, "unchanged") == 0);
    assert(output_sample_rate == 16000);

    const char no_session[] =
        "{\"type\":\"hello\",\"transport\":\"websocket\"," 
        "\"audio_params\":{\"sample_rate\":24000}}";
    session_id[0] = '\0';
    assert(!parse_xiaozhi_server_hello(no_session,
                                       strlen(no_session),
                                       session_id,
                                       sizeof(session_id),
                                       &output_sample_rate));
    assert(output_sample_rate == 24000);

    strlcpy(session_id, "existing-session", sizeof(session_id));
    output_sample_rate = 16000;
    assert(parse_xiaozhi_server_hello(no_session,
                                      strlen(no_session),
                                      session_id,
                                      sizeof(session_id),
                                      &output_sample_rate));
    assert(strcmp(session_id, "existing-session") == 0);
    assert(output_sample_rate == 24000);

    const char empty_session[] =
        "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"\"}";
    assert(!parse_xiaozhi_server_hello(empty_session,
                                       strlen(empty_session),
                                       session_id,
                                       sizeof(session_id),
                                       &output_sample_rate));
    assert(session_id[0] == '\0');

    const char unsupported_rate[] =
        "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"session-1\"," 
        "\"audio_params\":{\"sample_rate\":48000}}";
    output_sample_rate = 16000;
    assert(parse_xiaozhi_server_hello(unsupported_rate,
                                      strlen(unsupported_rate),
                                      session_id,
                                      sizeof(session_id),
                                      &output_sample_rate));
    assert(strcmp(session_id, "session-1") == 0);
    assert(output_sample_rate == 16000);

    const char supported_rate[] =
        "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"session-24k\"," 
        "\"audio_params\":{\"sample_rate\":24000}}";
    assert(parse_xiaozhi_server_hello(supported_rate,
                                      strlen(supported_rate),
                                      session_id,
                                      sizeof(session_id),
                                      &output_sample_rate));
    assert(strcmp(session_id, "session-24k") == 0);
    assert(output_sample_rate == 24000);

    char short_session_id[5] = {};
    assert(parse_xiaozhi_server_hello(supported_rate,
                                      strlen(supported_rate),
                                      short_session_id,
                                      sizeof(short_session_id),
                                      &output_sample_rate));
    assert(strcmp(short_session_id, "sess") == 0);

    return 0;
}
