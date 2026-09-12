// 管理 SDL 工作页共享的分段进度 canvas、像素缓冲和增量刷新状态。
#pragma once

#include <time.h>

#include <vector>

#include "lvgl.h"

namespace sdl_preview_progress {

class Canvas {
public:
    Canvas();

    Canvas(const Canvas &) = delete;
    Canvas &operator=(const Canvas &) = delete;

    void build(lv_obj_t *parent, int y);
    void build_day(lv_obj_t *parent, const struct tm &local, int y);
    void update(int filled);
    lv_obj_t *object() const;

private:
    lv_obj_t *canvas_ = nullptr;
    int last_filled_ = -1;
    std::vector<lv_color_t> pixels_;
};

}  // namespace sdl_preview_progress
