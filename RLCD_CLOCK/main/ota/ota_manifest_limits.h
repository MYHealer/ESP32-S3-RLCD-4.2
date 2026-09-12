// 集中定义 OTA manifest 固定文本字段的存储容量。
#pragma once

inline constexpr int kOtaVersionLen = 24;
inline constexpr int kOtaUrlLen = 256;
inline constexpr int kOtaSha256Len = 65;
