// 组装小智设备信息并执行官方激活 HTTP 请求，不修改会话状态。
#include "xiaozhi_activation_client.h"

#include "app_display_config.h"
#include "app_metadata.h"
#include "network_https_resources.h"
#include "network_task_guards.h"
#include "scoped_http_client.h"
#include "xiaozhi_activation_storage.h"

#include <esp_app_desc.h>
#include <esp_chip_info.h>
#include <esp_crt_bundle.h>
#include <esp_flash.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include <freertos/FreeRTOS.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kActivationHttpTimeoutMs = 12000;
constexpr const char *kActivationUrl = CONFIG_XIAOZHI_AI_OTA_URL;

#define XIAOZHI_ACTIVATION_HEADER_FAILED_FORMAT "xiaozhi activation header %s failed: %s"
#define XIAOZHI_ACTIVATION_BODY_FAILED_FORMAT "xiaozhi activation body failed: %s"

static_assert(kXiaozhiActivationRequestSize > 0,
              "Xiaozhi activation request buffer must be positive");
static_assert(kActivationHttpTimeoutMs > 0, "Xiaozhi activation timeout must be positive");
static_assert(kXiaozhiActivationResponseSize > 1,
              "Xiaozhi activation response must fit data and a terminator");
static_assert(kXiaozhiDeviceIdSize == sizeof("00:00:00:00:00:00"),
              "Xiaozhi device ID buffer must fit one MAC address plus terminator");

esp_err_t activation_http_event(esp_http_client_event_t *event)
{
    if (!event || event->event_id != HTTP_EVENT_ON_DATA || !event->user_data ||
        !event->data || event->data_len <= 0) {
        return ESP_OK;
    }
    XiaozhiActivationResponse *buffer =
        static_cast<XiaozhiActivationResponse *>(event->user_data);
    size_t room = xiaozhi_activation_response_writable_bytes(buffer);
    if (room == 0) {
        return ESP_OK;
    }
    size_t copy_len = static_cast<size_t>(event->data_len) < room
                          ? static_cast<size_t>(event->data_len)
                          : room;
    if (copy_len > 0) {
        memcpy(buffer->data + buffer->len, event->data, copy_len);
        buffer->len += copy_len;
        buffer->data[buffer->len] = '\0';
    }
    return ESP_OK;
}

bool set_activation_http_header(esp_http_client_handle_t client,
                                const char *name,
                                const char *value)
{
    esp_err_t err = esp_http_client_set_header(client, name, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_ACTIVATION_HEADER_FAILED_FORMAT, name, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool configure_activation_http_request(esp_http_client_handle_t client,
                                       const char *user_agent,
                                       const char *device_id,
                                       const char *client_id,
                                       const char *body)
{
    if (!set_activation_http_header(client, "Content-Type", "application/json") ||
        !set_activation_http_header(client, "Accept-Language", "zh-CN") ||
        !set_activation_http_header(client, "User-Agent", user_agent) ||
        !set_activation_http_header(client, "Activation-Version", "1") ||
        !set_activation_http_header(client, "Device-Id", device_id) ||
        !set_activation_http_header(client, "Client-Id", client_id)) {
        return false;
    }
    esp_err_t err = esp_http_client_set_post_field(client, body, strlen(body));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, XIAOZHI_ACTIVATION_BODY_FAILED_FORMAT, esp_err_to_name(err));
        return false;
    }
    return true;
}

struct ActivationHttpResult {
    esp_err_t err = ESP_FAIL;
    int status = 0;
};

bool perform_activation_http_request(const esp_http_client_config_t &config,
                                     const char *user_agent,
                                     const char *device_id,
                                     const char *client_id,
                                     const char *body,
                                     ActivationHttpResult *result)
{
    if (!result) {
        return false;
    }
    NetworkHttpTransactionGuard transaction_lock(
        pdMS_TO_TICKS(config.timeout_ms));
    if (!transaction_lock.locked()) {
        ESP_LOGW(TAG, "xiaozhi activation deferred: TLS session is busy");
        return false;
    }
    ScopedHttpClient client(&config);
    if (!client) {
        return false;
    }
    if (!configure_activation_http_request(client.get(),
                                           user_agent,
                                           device_id,
                                           client_id,
                                           body)) {
        return false;
    }
    {
        NetworkDisplayDmaGuard display_guard(true);
        result->err = esp_http_client_perform(client.get());
    }
    result->status = esp_http_client_get_status_code(client.get());
    return true;
}
} // namespace

void xiaozhi_format_device_id(char *out, size_t out_len)
{
    uint8_t mac[6] = {};
    if (!out || out_len == 0 || esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return;
    }
    snprintf(out,
             out_len,
             "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

bool xiaozhi_request_activation(XiaozhiActivationScratch *scratch)
{
    if (!scratch) {
        return false;
    }
    XiaozhiActivationResponse *response = &scratch->response;
    xiaozhi_reset_activation_response(response);
    scratch->request[0] = '\0';
    char device_id[kXiaozhiDeviceIdSize] = {};
    char client_id[kXiaozhiClientIdSize] = {};
    xiaozhi_format_device_id(device_id, sizeof(device_id));
    if (!xiaozhi_load_or_create_client_id(client_id, sizeof(client_id))) {
        return false;
    }
    uint32_t flash_size = 0;
    (void)esp_flash_get_size(nullptr, &flash_size);
    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    int written = snprintf(
        scratch->request,
        sizeof(scratch->request),
        "{\"version\":2,\"language\":\"zh-CN\",\"flash_size\":%lu,"
        "\"minimum_free_heap_size\":\"%lu\",\"mac_address\":\"%s\",\"uuid\":\"%s\","
        "\"chip_model_name\":\"esp32s3\",\"chip_info\":{\"model\":%d,\"cores\":%d,\"revision\":%d,\"features\":%lu},"
        "\"application\":{\"name\":\"%s\",\"version\":\"%s\",\"compile_time\":\"%sT%sZ\",\"idf_version\":\"%s\"},"
        "\"ota\":{\"label\":\"%s\"},\"display\":{\"monochrome\":true,\"width\":%d,\"height\":%d},"
        "\"board\":{\"type\":\"wifi\",\"name\":\"s3-rlcd-4.2\",\"mac\":\"%s\"}}",
        static_cast<unsigned long>(flash_size),
        static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
        device_id,
        client_id,
        static_cast<int>(chip_info.model),
        static_cast<int>(chip_info.cores),
        static_cast<int>(chip_info.revision),
        static_cast<unsigned long>(chip_info.features),
        app ? app->project_name : "weather_clock",
        app ? app->version : APP_VERSION,
        app ? app->date : __DATE__,
        app ? app->time : __TIME__,
        app ? app->idf_ver : "unknown",
        running ? running->label : "ota_0",
        kDisplayWidth,
        kDisplayHeight,
        device_id);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(scratch->request)) {
        return false;
    }
    esp_http_client_config_t config = {};
    config.url = kActivationUrl;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = kActivationHttpTimeoutMs;
    config.event_handler = activation_http_event;
    config.user_data = response;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    char user_agent[64] = {};
    snprintf(user_agent, sizeof(user_agent), "s3-rlcd-4.2/%s", app ? app->version : APP_VERSION);
    ActivationHttpResult result;
    if (!perform_activation_http_request(config,
                                         user_agent,
                                         device_id,
                                         client_id,
                                         scratch->request,
                                         &result)) {
        return false;
    }
    ESP_LOGI(TAG,
             "xiaozhi activation result: status=%d err=%s response_len=%u",
             result.status,
             esp_err_to_name(result.err),
             static_cast<unsigned>(response->len));
    return result.err == ESP_OK && result.status == 200 && response->len > 0;
}
