// 验证小智 MCP 请求信封解析和有界 token 预筛选。
#include "xiaozhi_mcp_request_parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void test_token_filter()
{
    expect(!xiaozhi_mcp_json_token_present(nullptr, 0), "null token input accepted");
    expect(!xiaozhi_mcp_json_token_present("mcp", 3), "unquoted token accepted");
    constexpr char kExact[] = "\"mcp\"";
    expect(xiaozhi_mcp_json_token_present(kExact, sizeof(kExact) - 1), "exact token rejected");
    constexpr char kEmbedded[] = "xx\"mcp\"yy";
    expect(xiaozhi_mcp_json_token_present(kEmbedded, sizeof(kEmbedded) - 1),
           "embedded token rejected");
    expect(!xiaozhi_mcp_json_token_present(kEmbedded, 6), "token scan exceeded supplied length");
}

void test_invalid_and_non_mcp_documents()
{
    XiaozhiMcpRequestDocument document;
    expect(!document.parse(nullptr, 0), "null document parsed");
    expect(!document.parse("{", 1), "malformed document parsed");
    constexpr char kNonMcp[] =
        "{\"type\":\"stt\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"id\":1}}";
    expect(document.parse(kNonMcp, sizeof(kNonMcp) - 1), "non-MCP JSON did not parse");
    expect(document.type() && std::strcmp(document.type(), "stt") == 0, "type mismatch");
    expect(document.version() && std::strcmp(document.version(), "2.0") == 0,
           "version mismatch");
    expect(document.method() && std::strcmp(document.method(), "initialize") == 0,
           "method mismatch");
    expect(cJSON_IsNumber(document.id()), "numeric id missing");
    expect(document.params() == nullptr, "missing params was synthesized");
    expect(document.tool_name() == nullptr, "missing tool name was synthesized");
}

void test_tool_call_fields()
{
    constexpr char kRequest[] =
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\","
        "\"id\":\"request-7\",\"params\":{\"name\":\"self.weather.set_city\","
        "\"arguments\":{\"city\":\"杭州\"}}}}";
    XiaozhiMcpRequestDocument document;
    expect(document.parse(kRequest, sizeof(kRequest) - 1), "tool call did not parse");
    expect(document.type() && std::strcmp(document.type(), "mcp") == 0, "MCP type mismatch");
    expect(document.version() && std::strcmp(document.version(), "2.0") == 0,
           "tool call version mismatch");
    expect(document.method() && std::strcmp(document.method(), "tools/call") == 0,
           "tool call method mismatch");
    expect(cJSON_IsString(document.id()) &&
               std::strcmp(document.id()->valuestring, "request-7") == 0,
           "string id mismatch");
    expect(cJSON_IsObject(document.params()), "tool params missing");
    expect(document.tool_name() &&
               std::strcmp(document.tool_name(), "self.weather.set_city") == 0,
           "tool name mismatch");
}

void test_wrong_field_types_and_reparse()
{
    constexpr char kWrong[] =
        "{\"type\":1,\"payload\":{\"jsonrpc\":2,\"method\":false,\"id\":{},"
        "\"params\":\"bad\"}}";
    XiaozhiMcpRequestDocument document;
    expect(document.parse(kWrong, sizeof(kWrong) - 1), "wrong-type JSON did not parse");
    expect(document.type() == nullptr, "numeric type accepted");
    expect(document.version() == nullptr, "numeric version accepted");
    expect(document.method() == nullptr, "boolean method accepted");
    expect(cJSON_IsObject(document.id()), "raw invalid id was not preserved");
    expect(cJSON_IsString(document.params()), "raw invalid params was not preserved");
    expect(document.tool_name() == nullptr, "tool name read from non-object params");

    constexpr char kEmpty[] = "{}";
    expect(document.parse(kEmpty, sizeof(kEmpty) - 1), "empty object did not parse");
    expect(document.type() == nullptr && document.version() == nullptr &&
               document.method() == nullptr && document.id() == nullptr &&
               document.params() == nullptr && document.tool_name() == nullptr,
           "reparse retained stale field pointers");
}
} // namespace

int main()
{
    test_token_filter();
    test_invalid_and_non_mcp_documents();
    test_tool_call_fields();
    test_wrong_field_types_and_reparse();
    std::puts("Xiaozhi MCP request parser host tests passed");
    return 0;
}
