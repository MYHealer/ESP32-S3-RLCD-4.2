// 在主机验证 QWeather 响应缓冲、JSON 生命周期和业务成功字段边界。
#include "qweather_response.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace {

void test_stage_and_response_buffer()
{
    assert(strcmp(qweather_stage_text(nullptr), "request") == 0);
    assert(strcmp(qweather_stage_text(""), "request") == 0);
    assert(strcmp(qweather_stage_text("daily"), "daily") == 0);

    QweatherResponseBuffer empty("test", 0);
    assert(!empty);
    assert(empty.get() == nullptr);
    assert(empty.size() == 0);

    QweatherResponseBuffer response("test", 32);
    assert(response);
    assert(response.get() != nullptr);
    assert(response.size() == 32);
    assert(response.get()[0] == '\0');

    QweatherResponseBuffer exchange("test", 32, 64);
    assert(exchange);
    assert(exchange.request_url() != nullptr);
    assert(exchange.request_url_size() == 64);
    assert(exchange.get() == exchange.request_url() + 64);
    assert(exchange.size() == 32);
    assert(exchange.request_url()[0] == '\0');
    assert(exchange.get()[0] == '\0');
    exchange.request_url()[0] = 'u';
    exchange.get()[0] = 'r';
    assert(exchange.request_url()[0] == 'u');
    assert(exchange.get()[0] == 'r');

    QweatherResponseBuffer staged("test", 32, 64, 16);
    assert(staged);
    assert(staged.request_url() != nullptr);
    assert(staged.get() == staged.request_url() + 64);
    assert(staged.staging() ==
           static_cast<void *>(staged.get() + staged.size()));
    assert(staged.staging_size() == 16);
    memset(staged.staging(), 0x5a, staged.staging_size());
    assert(static_cast<unsigned char *>(staged.staging())[15] == 0x5a);

    QweatherResponseBuffer overflow("test", SIZE_MAX, 1);
    assert(!overflow);
    assert(overflow.request_url() == nullptr);
    assert(overflow.get() == nullptr);

    QweatherResponseBuffer staging_overflow("test", 1, 1, SIZE_MAX);
    assert(!staging_overflow);
    assert(staging_overflow.request_url() == nullptr);
    assert(staging_overflow.get() == nullptr);
    assert(staging_overflow.staging() == nullptr);
}

void test_json_root_rejects_invalid_input()
{
    QweatherJsonRoot null_root(nullptr);
    assert(!null_root);
    assert(null_root.get() == nullptr);

    char invalid[] = "{";
    QweatherJsonRoot invalid_root(invalid);
    assert(!invalid_root);
}

void test_json_root_accepts_read_only_text()
{
    NetworkJsonRoot root(R"({"code":"200"})");
    assert(root);
    assert(qweather_code_ok(cJSON_GetObjectItem(root.get(), "code")));
}

void test_success_object_and_array()
{
    char json[] = R"({"code":"200","now":{"temp":"26"},"daily":[{"fxDate":"2026-07-13"}]})";
    QweatherJsonRoot root(json);
    assert(root);

    const cJSON *code = nullptr;
    const cJSON *now = qweather_success_object(root.get(), "now", &code);
    assert(now != nullptr);
    assert(qweather_code_ok(code));
    assert(strcmp(qweather_code_text(code), "200") == 0);

    const cJSON *daily = qweather_success_array(root.get(), "daily", nullptr);
    assert(daily != nullptr);
    assert(cJSON_GetArraySize(daily) == 1);
    assert(qweather_alert_success_array(root.get(), "daily", nullptr) == daily);

    assert(qweather_success_array(root.get(), "now", nullptr) == nullptr);
    assert(qweather_success_object(root.get(), "daily", nullptr) == nullptr);
}

void test_failed_and_missing_business_code()
{
    char failed_json[] = R"({"code":"401","now":{"temp":"26"}})";
    QweatherJsonRoot failed_root(failed_json);
    assert(failed_root);
    const cJSON *code = nullptr;
    assert(qweather_success_item(failed_root.get(), "now", &code) == nullptr);
    assert(!qweather_code_ok(code));
    assert(strcmp(qweather_code_text(code), "401") == 0);

    char missing_json[] = R"({"now":{"temp":"26"}})";
    QweatherJsonRoot missing_root(missing_json);
    assert(missing_root);
    code = reinterpret_cast<const cJSON *>(1);
    assert(qweather_success_item(missing_root.get(), "now", &code) == nullptr);
    assert(code == nullptr);
    assert(strcmp(qweather_code_text(code), "missing") == 0);

    code = reinterpret_cast<const cJSON *>(1);
    assert(qweather_success_item(nullptr, "now", &code) == nullptr);
    assert(code == nullptr);
    assert(qweather_success_item(missing_root.get(), nullptr, nullptr) == nullptr);
    assert(qweather_json_string_value(nullptr) == nullptr);

    char numeric_code_json[] = R"({"code":200,"now":{"temp":"26"}})";
    QweatherJsonRoot numeric_code_root(numeric_code_json);
    assert(numeric_code_root);
    code = reinterpret_cast<const cJSON *>(1);
    assert(qweather_success_item(numeric_code_root.get(), "now", &code) == nullptr);
    assert(code != nullptr);
    assert(qweather_json_string_value(code) == nullptr);
    assert(strcmp(qweather_code_text(code), "missing") == 0);
}

void test_alert_v2_response()
{
    const cJSON *code = reinterpret_cast<const cJSON *>(1);
    NetworkJsonRoot empty(
        R"({"metadata":{"tag":"abc","zeroResult":true},"alerts":[]})");
    const cJSON *alerts =
        qweather_alert_success_array(empty.get(), "alerts", &code);
    assert(cJSON_IsArray(alerts));
    assert(cJSON_GetArraySize(alerts) == 0);
    assert(code == nullptr);

    NetworkJsonRoot active(
        R"({"metadata":{"zeroResult":false},"alerts":[{"headline":"大风预警"}]})");
    alerts = qweather_alert_success_array(active.get(), "alerts", nullptr);
    assert(cJSON_IsArray(alerts));
    assert(cJSON_GetArraySize(alerts) == 1);

    NetworkJsonRoot missing_metadata(R"({"alerts":[]})");
    assert(qweather_alert_success_array(
               missing_metadata.get(), "alerts", nullptr) == nullptr);
    NetworkJsonRoot missing_zero_result(
        R"({"metadata":{},"alerts":[]})");
    assert(qweather_alert_success_array(
               missing_zero_result.get(), "alerts", nullptr) == nullptr);
    NetworkJsonRoot invalid_zero_result(
        R"({"metadata":{"zeroResult":"true"},"alerts":[]})");
    assert(qweather_alert_success_array(
               invalid_zero_result.get(), "alerts", nullptr) == nullptr);
    NetworkJsonRoot invalid_array(
        R"({"metadata":{"zeroResult":true},"alerts":{}})");
    assert(qweather_alert_success_array(
               invalid_array.get(), "alerts", nullptr) == nullptr);

    NetworkJsonRoot v1_error_with_v2_shape(
        R"({"code":"401","metadata":{"zeroResult":true},"alerts":[]})");
    code = nullptr;
    assert(qweather_alert_success_array(
               v1_error_with_v2_shape.get(), "alerts", &code) == nullptr);
    assert(strcmp(qweather_code_text(code), "401") == 0);
}

} // namespace

int main()
{
    test_stage_and_response_buffer();
    test_json_root_rejects_invalid_input();
    test_json_root_accepts_read_only_text();
    test_success_object_and_array();
    test_failed_and_missing_business_code();
    test_alert_v2_response();
    return 0;
}
