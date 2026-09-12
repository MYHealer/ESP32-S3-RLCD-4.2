// 声明配网页 HTML、扫描列表和结果页渲染接口。
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "wifi_portal_state.h"

#include <stddef.h>

void html_append(char *html, size_t html_len, const char *fmt, ...);
void append_wifi_scan_list(char *html, size_t html_len);
esp_err_t root_get_handler(httpd_req_t *req);
esp_err_t send_save_result_page(httpd_req_t *req,
                                WifiPortalSaveResult result,
                                const char *extra_message = nullptr);
esp_err_t send_offline_result_page(httpd_req_t *req, bool saved);
esp_err_t send_portal_text_status(httpd_req_t *req, const char *status, const char *text);
esp_err_t send_portal_empty_status(httpd_req_t *req, const char *status);
esp_err_t redirect_to_setup_portal(httpd_req_t *req);
