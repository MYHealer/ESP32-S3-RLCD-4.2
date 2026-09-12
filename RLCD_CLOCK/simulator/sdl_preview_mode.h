// 解析 SDL 预览模式并分类需要构建的工作页。
#pragma once

#include <string.h>

namespace sdl_preview_mode {

inline bool is(const char *mode, const char *expected)
{
    return mode && expected && strcmp(mode, expected) == 0;
}

inline bool has_prefix(const char *mode, const char *prefix)
{
    return mode && prefix && strncmp(mode, prefix, strlen(prefix)) == 0;
}

struct Selection {
    bool history = false;
    bool gallery = false;
    bool flip_clock = false;
    bool xiaozhi = false;
    bool calendar = false;
    bool weather_board = false;
    bool info = false;
    bool aggregate_clock = false;

    bool alternate_work_page() const
    {
        return history || gallery || flip_clock || xiaozhi || calendar ||
               weather_board || info || aggregate_clock;
    }
};

inline Selection selection_for(const char *mode)
{
    return {
        is(mode, "history"),
        is(mode, "gallery"),
        is(mode, "flip_clock"),
        has_prefix(mode, "xiaozhi"),
        is(mode, "calendar"),
        is(mode, "weather_board"),
        is(mode, "info"),
        is(mode, "aggregate_clock"),
    };
}

inline bool is_settings(const char *mode)
{
    return is(mode, "settings") || has_prefix(mode, "settings_");
}

} // namespace sdl_preview_mode
