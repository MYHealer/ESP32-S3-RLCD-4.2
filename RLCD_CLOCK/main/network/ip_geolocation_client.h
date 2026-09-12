// 声明第三方 IP 定位查询的轻量客户端接口。
#pragma once

#include <stddef.h>

bool ip_geolocation_lookup(char *location,
                           size_t location_len,
                           char *city,
                           size_t city_len);
