// 验证正常局部刷新保持静默，仅异常全屏刷新输出诊断。
#include "ui_display_diag_policy.h"

#include <assert.h>

int main()
{
    DisplayFlushDiagDecision decision = display_flush_diag_decision(
        true, false, false, 0, false, false);
    assert(decision.close_window);
    assert(!decision.emit_log);

    decision = display_flush_diag_decision(false, true, false, 0, false, false);
    assert(decision.close_window);
    assert(!decision.emit_log);

    decision = display_flush_diag_decision(false, false, true, 0, false, false);
    assert(decision.close_window);
    assert(!decision.emit_log);

    decision = display_flush_diag_decision(false, true, false, 2, false, false);
    assert(decision.close_window);
    assert(decision.emit_log);

    decision = display_flush_diag_decision(false, false, false, 2, false, false);
    assert(!decision.close_window);
    assert(!decision.emit_log);

    decision = display_flush_diag_decision(false, true, false, 2, true, false);
    assert(decision.close_window);
    assert(!decision.emit_log);

    decision = display_flush_diag_decision(false, true, false, 2, false, true);
    assert(decision.close_window);
    assert(!decision.emit_log);
    return 0;
}
