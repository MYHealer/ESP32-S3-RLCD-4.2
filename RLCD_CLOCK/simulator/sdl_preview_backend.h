// 封装 SDL 预览窗口、ARGB framebuffer、LVGL flush 和 PPM 截图资源。
#pragma once

#include <SDL.h>
#include <stdint.h>

#include <vector>

#include "lvgl.h"

struct SdlPreviewBackend {
    explicit SdlPreviewBackend(int display_width, int display_height);

    int width;
    int height;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    std::vector<uint32_t> framebuffer;
};

bool sdl_preview_backend_init(SdlPreviewBackend *backend,
                              const char *window_title,
                              int window_scale);
void sdl_preview_backend_flush(SdlPreviewBackend *backend,
                               lv_disp_drv_t *driver,
                               const lv_area_t *area,
                               lv_color_t *color);
void sdl_preview_backend_save_ppm(const SdlPreviewBackend *backend, const char *path);
void sdl_preview_backend_cleanup(SdlPreviewBackend *backend);
