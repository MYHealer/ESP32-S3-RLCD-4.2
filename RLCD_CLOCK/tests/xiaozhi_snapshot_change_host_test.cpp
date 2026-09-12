// 验证小智快照任一可见字段变化都会被识别。
#include "xiaozhi_snapshot_change.h"

#include <assert.h>

namespace {
XiaozhiAiSnapshot ready_snapshot()
{
    XiaozhiAiSnapshot value = {};
    value.state = kXiaozhiAiReady;
    strlcpy(value.status, "ready", sizeof(value.status));
    strlcpy(value.detail, "wake word", sizeof(value.detail));
    strlcpy(value.binding_code, "123456", sizeof(value.binding_code));
    strlcpy(value.emotion, "neutral", sizeof(value.emotion));
    value.waveform_level = 0;
    value.activity_sequence = 7;
    return value;
}
} // namespace

int main()
{
    const XiaozhiAiSnapshot baseline = ready_snapshot();
    XiaozhiAiSnapshot changed = baseline;
    assert(xiaozhi_snapshot_content_equal(baseline, changed));

    changed.state = kXiaozhiAiListening;
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    strlcpy(changed.status, "listening", sizeof(changed.status));
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    strlcpy(changed.detail, "changed", sizeof(changed.detail));
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    strlcpy(changed.binding_code, "654321", sizeof(changed.binding_code));
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    strlcpy(changed.emotion, "happy", sizeof(changed.emotion));
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    changed.waveform_level = 1;
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    changed = baseline;
    ++changed.activity_sequence;
    assert(!xiaozhi_snapshot_content_equal(baseline, changed));
    return 0;
}
