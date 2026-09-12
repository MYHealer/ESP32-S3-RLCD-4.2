// 验证设置页导航状态始终以完整快照跨任务发布。
#include "ui_settings_navigation_state_internal.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

namespace {
bool snapshots_equal(const SettingsNavigationSnapshot &lhs,
                     const SettingsNavigationSnapshot &rhs)
{
    return lhs.focus_secondary == rhs.focus_secondary &&
           lhs.page_toggle_mode == rhs.page_toggle_mode &&
           lhs.page_order_mode == rhs.page_order_mode &&
           lhs.primary_selection == rhs.primary_selection &&
           lhs.selection == rhs.selection &&
           lhs.page_order_selection == rhs.page_order_selection;
}
} // namespace

int main()
{
    const SettingsNavigationSnapshot initial = settings_navigation_snapshot();
    assert(!initial.focus_secondary);
    assert(!initial.page_toggle_mode);
    assert(!initial.page_order_mode);
    assert(initial.primary_selection == 0);
    assert(initial.selection == 0);
    assert(initial.page_order_selection == 0);

    SettingsNavigationSnapshot first;
    first.focus_secondary = true;
    first.page_toggle_mode = true;
    first.primary_selection = 2;
    first.selection = 6;
    first.page_order_selection = 4;

    SettingsNavigationSnapshot second;
    second.page_order_mode = true;
    second.primary_selection = 3;
    second.selection = 1;
    second.page_order_selection = 5;

    settings_navigation_store(first);
    assert(snapshots_equal(settings_navigation_snapshot(), first));
    settings_navigation_store(second);
    assert(snapshots_equal(settings_navigation_snapshot(), second));

    std::atomic<bool> start{false};
    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < 200000; ++i) {
            settings_navigation_store((i & 1) ? first : second);
        }
        writer_done.store(true, std::memory_order_release);
    });

    start.store(true, std::memory_order_release);
    while (!writer_done.load(std::memory_order_acquire)) {
        const SettingsNavigationSnapshot observed = settings_navigation_snapshot();
        assert(snapshots_equal(observed, first) || snapshots_equal(observed, second));
    }
    writer.join();

    std::puts("Settings navigation snapshot host tests passed");
    return 0;
}
