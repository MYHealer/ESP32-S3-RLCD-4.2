// 实现 SDL 启动页和启动动画的独立预览生命周期。
#include "sdl_preview_boot.h"

#include "boot_anim.h"
#include "sdl_preview_backend.h"
#include "sdl_preview_mode.h"
#include "sdl_preview_widgets.h"

#include <SDL.h>

#include <vector>

namespace {

constexpr uint32_t kBootAnimRunFrameMs = 50;

lv_obj_t *g_boot_anim_canvas = nullptr;
std::vector<lv_color_t> g_boot_anim_canvas_pixels(BOOT_ANIM_WIDTH * BOOT_ANIM_HEIGHT);

void draw_boot_anim_frame_index(int frame)
{
    if (!g_boot_anim_canvas) {
        return;
    }
    if (frame < 0) {
        frame = 0;
    } else if (frame >= BOOT_ANIM_FRAME_COUNT) {
        frame = BOOT_ANIM_FRAME_COUNT - 1;
    }
    const uint8_t *pixels = boot_anim_frames[frame];
    uint32_t bit = 0;
    for (int y = 0; y < BOOT_ANIM_HEIGHT; ++y) {
        for (int x = 0; x < BOOT_ANIM_WIDTH; ++x, ++bit) {
            const bool black = pixels[bit / 8] & (0x80 >> (bit & 7));
            lv_canvas_set_px_color(g_boot_anim_canvas,
                                   x,
                                   y,
                                   black ? lv_color_black() : lv_color_white());
        }
    }
    lv_obj_invalidate(g_boot_anim_canvas);
}

void settle_preview_frame()
{
    for (int i = 0; i < 5; ++i) {
        lv_tick_inc(16);
        lv_timer_handler();
        SDL_Delay(16);
    }
}

} // namespace

void build_boot_preview_screen(const char *app_version)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = sdl_preview_widgets::make_label_with_font(
        screen, 28, 30, 344, 30, "RLCD Weather Clock", &lv_font_montserrat_16);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *status = sdl_preview_widgets::make_label_with_font(
        screen, 28, 64, 344, 24, "Starting...", &lv_font_montserrat_16);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *version = sdl_preview_widgets::make_label_with_font(
        screen, 28, 226, 344, 24, app_version ? app_version : "", &lv_font_montserrat_16);
    lv_obj_set_style_text_align(version, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    g_boot_anim_canvas = lv_canvas_create(screen);
    lv_obj_clear_flag(g_boot_anim_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(g_boot_anim_canvas, 144, 100);
    lv_obj_set_size(g_boot_anim_canvas, BOOT_ANIM_WIDTH, BOOT_ANIM_HEIGHT);
    lv_obj_set_style_border_width(g_boot_anim_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_boot_anim_canvas, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(g_boot_anim_canvas,
                         g_boot_anim_canvas_pixels.data(),
                         BOOT_ANIM_WIDTH,
                         BOOT_ANIM_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(g_boot_anim_canvas, lv_color_white(), LV_OPA_COVER);
    draw_boot_anim_frame_index(0);
}

bool save_boot_preview_if_requested(SdlPreviewBackend *backend,
                                    const char *screenshot_path,
                                    const char *preview_mode)
{
    if (!backend || !screenshot_path || !screenshot_path[0] ||
        !sdl_preview_mode::is(preview_mode, "boot")) {
        return false;
    }
    settle_preview_frame();
    sdl_preview_backend_save_ppm(backend, screenshot_path);
    sdl_preview_backend_cleanup(backend);
    return true;
}

void run_boot_preview_animation()
{
    const uint32_t boot_start = SDL_GetTicks();
    uint32_t boot_last_tick = boot_start;
    uint32_t boot_last_frame_tick = boot_start;
    int boot_anim_frame = 0;
    while (SDL_GetTicks() - boot_start < 3000) {
        const uint32_t now_tick = SDL_GetTicks();
        lv_tick_inc(now_tick - boot_last_tick);
        boot_last_tick = now_tick;
        if (now_tick - boot_last_frame_tick >= kBootAnimRunFrameMs) {
            boot_last_frame_tick = now_tick;
            boot_anim_frame = (boot_anim_frame + 1) % BOOT_ANIM_FRAME_COUNT;
        }
        draw_boot_anim_frame_index(boot_anim_frame);
        lv_timer_handler();
        SDL_Delay(30);
    }
    draw_boot_anim_frame_index(BOOT_ANIM_FRAME_COUNT - 1);
    lv_timer_handler();
    SDL_Delay(100);
    lv_obj_clean(lv_scr_act());
    g_boot_anim_canvas = nullptr;
}
