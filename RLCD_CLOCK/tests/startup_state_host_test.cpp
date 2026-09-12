// 验证启动画面状态保持初始活跃并只能单向结束。
#include "startup_state_internal.h"

#include <cassert>
#include <cstdio>

int main()
{
    assert(startup_screen_active());
    startup_screen_mark_finished();
    assert(!startup_screen_active());
    startup_screen_mark_finished();
    assert(!startup_screen_active());
    std::puts("Startup state host tests passed");
    return 0;
}
