// 验证小智激活响应中绑定字段与 WebSocket 节点的只读提取语义。
#include "xiaozhi_activation_client.h"
#include "xiaozhi_activation_response_parser.h"

#include <assert.h>
#include <string.h>

int main()
{
    xiaozhi_reset_activation_response(nullptr);
    assert(xiaozhi_activation_response_writable_bytes(nullptr) == 0);
    XiaozhiActivationResponse response;
    assert(xiaozhi_activation_response_writable_bytes(&response) ==
           kXiaozhiActivationResponseSize - 1);
    response.len = kXiaozhiActivationResponseSize - 2;
    assert(xiaozhi_activation_response_writable_bytes(&response) == 1);
    response.len = kXiaozhiActivationResponseSize - 1;
    assert(xiaozhi_activation_response_writable_bytes(&response) == 0);
    response.len = kXiaozhiActivationResponseSize;
    assert(xiaozhi_activation_response_writable_bytes(&response) == 0);
    response.len = static_cast<size_t>(-1);
    assert(xiaozhi_activation_response_writable_bytes(&response) == 0);
    response.data[0] = 'x';
    response.data[1] = '\0';
    xiaozhi_reset_activation_response(&response);
    assert(response.len == 0);
    assert(response.data[0] == '\0');
    assert(xiaozhi_activation_response_writable_bytes(&response) ==
           kXiaozhiActivationResponseSize - 1);

    XiaozhiActivationResponseDocument document;

    assert(!document.parse(nullptr, 0));
    assert(!document.parse("{", 1));
    assert(document.message() == nullptr);
    assert(document.binding_code() == nullptr);
    assert(document.challenge() == nullptr);
    assert(document.websocket() == nullptr);

    const char empty[] = "{}";
    assert(document.parse(empty, sizeof(empty) - 1));
    assert(document.message() == nullptr);
    assert(document.binding_code() == nullptr);
    assert(document.challenge() == nullptr);
    assert(document.websocket() == nullptr);

    const char binding[] =
        "{\"activation\":{\"message\":\"请绑定\",\"code\":\"123456\","
        "\"challenge\":\"challenge-value\"},"
        "\"websocket\":{\"url\":\"wss://example.test/ws\"}}";
    assert(document.parse(binding, sizeof(binding) - 1));
    assert(strcmp(document.message(), "请绑定") == 0);
    assert(strcmp(document.binding_code(), "123456") == 0);
    assert(strcmp(document.challenge(), "challenge-value") == 0);
    assert(cJSON_IsObject(document.websocket()));
    const cJSON *url = cJSON_GetObjectItem(document.websocket(), "url");
    assert(cJSON_IsString(url) && strcmp(url->valuestring, "wss://example.test/ws") == 0);

    const char wrong_activation_types[] =
        "{\"activation\":{\"message\":1,\"code\":false,\"challenge\":{}},"
        "\"websocket\":\"not-an-object\"}";
    assert(document.parse(wrong_activation_types, sizeof(wrong_activation_types) - 1));
    assert(document.message() == nullptr);
    assert(document.binding_code() == nullptr);
    assert(document.challenge() == nullptr);
    assert(cJSON_IsString(document.websocket()));

    const char activation_not_object[] =
        "{\"activation\":\"invalid\",\"websocket\":{\"version\":1}}";
    assert(document.parse(activation_not_object, sizeof(activation_not_object) - 1));
    assert(document.message() == nullptr);
    assert(document.binding_code() == nullptr);
    assert(document.challenge() == nullptr);
    assert(cJSON_IsObject(document.websocket()));

    const char empty_strings[] =
        "{\"activation\":{\"message\":\"\",\"code\":\"\",\"challenge\":\"\"}}";
    assert(document.parse(empty_strings, sizeof(empty_strings) - 1));
    assert(strcmp(document.message(), "") == 0);
    assert(strcmp(document.binding_code(), "") == 0);
    assert(strcmp(document.challenge(), "") == 0);
    assert(document.websocket() == nullptr);
    return 0;
}
