// 声明小智绑定码一次性语音播报及其纯筛选规则。
#pragma once

#include <cstring>

namespace xiaozhi_binding_voice {
inline bool should_announce(const char *binding_code, const char *last_announced)
{
    return binding_code && binding_code[0] != '\0' &&
           (!last_announced || std::strcmp(binding_code, last_announced) != 0);
}

inline int digit_index(char character)
{
    return character >= '0' && character <= '9' ? character - '0' : -1;
}
} // namespace xiaozhi_binding_voice

void xiaozhi_announce_binding_id_once(const char *binding_code);
