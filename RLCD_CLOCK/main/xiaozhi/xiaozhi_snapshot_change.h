// 定义小智页面快照最终可见内容的纯比较规则。
#pragma once

#include "xiaozhi_ai.h"

#include <string.h>

inline bool xiaozhi_snapshot_content_equal(const XiaozhiAiSnapshot &lhs,
                                           const XiaozhiAiSnapshot &rhs)
{
    return lhs.state == rhs.state &&
           strncmp(lhs.status, rhs.status, sizeof(lhs.status)) == 0 &&
           strncmp(lhs.detail, rhs.detail, sizeof(lhs.detail)) == 0 &&
           strncmp(lhs.binding_code, rhs.binding_code, sizeof(lhs.binding_code)) == 0 &&
           strncmp(lhs.emotion, rhs.emotion, sizeof(lhs.emotion)) == 0 &&
           lhs.waveform_level == rhs.waveform_level &&
           lhs.activity_sequence == rhs.activity_sequence;
}
