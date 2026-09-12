// 声明配网页联网凭据和离线日期时间的表单提交入口。
#pragma once

bool save_credentials_from_body(const char *body);
bool save_offline_datetime_from_body(const char *body);
