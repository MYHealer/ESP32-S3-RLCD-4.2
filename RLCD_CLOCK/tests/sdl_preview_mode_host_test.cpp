// 验证 SDL 预览模式与页面分类保持稳定映射。
#include "sdl_preview_mode.h"

#include <cassert>

int main()
{
    assert(!sdl_preview_mode::is(nullptr, "main"));
    assert(!sdl_preview_mode::is("main", nullptr));
    assert(sdl_preview_mode::is("main", "main"));
    assert(!sdl_preview_mode::is("main_extra", "main"));

    const auto none = sdl_preview_mode::selection_for(nullptr);
    assert(!none.alternate_work_page());
    assert(!sdl_preview_mode::selection_for("main").alternate_work_page());
    assert(sdl_preview_mode::selection_for("history").history);
    assert(sdl_preview_mode::selection_for("gallery").gallery);
    assert(sdl_preview_mode::selection_for("flip_clock").flip_clock);
    assert(sdl_preview_mode::selection_for("xiaozhi").xiaozhi);
    assert(sdl_preview_mode::selection_for("xiaozhi_preparing").xiaozhi);
    assert(sdl_preview_mode::selection_for("calendar").calendar);
    assert(sdl_preview_mode::selection_for("weather_board").weather_board);
    assert(sdl_preview_mode::selection_for("info").info);
    assert(!sdl_preview_mode::selection_for("history_extra").alternate_work_page());

    assert(sdl_preview_mode::is_settings("settings"));
    assert(sdl_preview_mode::is_settings("settings_network"));
    assert(!sdl_preview_mode::is_settings("settingsx"));
    assert(!sdl_preview_mode::is_settings(nullptr));
    return 0;
}
