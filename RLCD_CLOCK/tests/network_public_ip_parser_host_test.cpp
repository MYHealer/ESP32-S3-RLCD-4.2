// 验证网络检测公网 IPv4 响应解析、递归候选和错误输入边界。
#include "network_public_ip_parser.h"

#include <assert.h>
#include <string.h>

namespace {
void expect_ip(const char *response, const char *expected)
{
    char out[48] = {};
    assert(network_public_ip_parse_response(response, out, sizeof(out)));
    assert(strcmp(out, expected) == 0);
}

void expect_rejected(const char *response)
{
    char out[48] = "keep";
    assert(!network_public_ip_parse_response(response, out, sizeof(out)));
    assert(strcmp(out, "keep") == 0);
}
} // namespace

int main()
{
    expect_ip("203.0.113.7", "203.0.113.7");
    expect_ip(" \r\n203.0.113.8\t", "203.0.113.8");
    expect_ip("203.0.113.8 provider-response", "203.0.113.8");
    expect_ip(R"({"ip":"203.0.113.9"})", "203.0.113.9");
    expect_ip(R"({"ip":" 203.0.113.10 "})", "203.0.113.10");
    expect_ip(R"({"data":{"ip":"198.51.100.5"}})", "198.51.100.5");
    expect_ip(R"({"items":[{"ip":"192.0.2.6"}]})", "192.0.2.6");
    expect_ip(R"("192.0.2.7")", "192.0.2.7");
    expect_ip(R"({"ip":" ","data":{"ip":"198.51.100.6"}})",
              "198.51.100.6");
    expect_ip(R"({"ip":"error.example","data":{"ip":"198.51.100.7"}})",
              "198.51.100.7");

    expect_rejected(nullptr);
    expect_rejected("");
    expect_rejected("error.example");
    expect_rejected("status.invalid response");
    expect_rejected(R"({"ip":"error.example"})");
    expect_rejected(R"({"ip":"203.0.113.10 provider-response"})");
    expect_rejected(R"("203.0.113.10 provider-response")");
    expect_rejected(R"({"IP":"203.0.113.10"})");
    expect_rejected(R"({"message":"203.0.113.11"})");
    expect_rejected("256.0.0.1");
    expect_rejected("203.0.113");
    expect_rejected("203.0.113.1.5");
    expect_rejected("203.0.113.1/path");

    char short_out[8] = "keep";
    assert(!network_public_ip_parse_response("203.0.113.12",
                                             short_out,
                                             sizeof(short_out)));
    assert(strcmp(short_out, "keep") == 0);
    assert(!network_public_ip_parse_response("203.0.113.12", nullptr, 16));
    assert(!network_public_ip_parse_response("203.0.113.12", short_out, 0));
    return 0;
}
