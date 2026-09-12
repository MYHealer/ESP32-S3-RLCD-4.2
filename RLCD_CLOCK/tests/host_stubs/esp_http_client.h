// 为 ScopedHttpClient 主机测试声明最小 esp_http_client 接口。
#pragma once

struct esp_http_client_config_t {
    int marker = 0;
};

struct HostHttpClient;
using esp_http_client_handle_t = HostHttpClient *;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
void esp_http_client_cleanup(esp_http_client_handle_t client);
