// 声明手动天气城市的输入校验、空白清理和中文“市”后缀归一化。
#pragma once

#include <cstddef>

namespace weather_city_text {
bool input_valid(const char *city, size_t maximum_bytes);
bool normalize(const char *city, char *out, size_t out_len);
} // namespace weather_city_text
