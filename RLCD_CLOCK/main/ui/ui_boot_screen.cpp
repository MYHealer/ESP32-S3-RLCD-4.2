// 构建启动屏、播放启动动画并在启动完成后切换到首个已启用工作页。
#include "ui_boot_screen.h"

#include "active_work_page_state_internal.h"
#include "app_event_group.h"
#include "app_metadata.h"
#include "app_runtime_timing.h"
#include "boot_anim.h"
#include "lvgl_bsp.h"
#include "ui_bitmap.h"
#include "ui_canvas_primitives.h"
#include "ui_object_refs.h"
#include "ui_page_state.h"
#include "ui_widgets.h"
#include "ui_work_page_catalog.h"

#include <atomic>
#include <cstdlib>
#include <esp_log.h>

namespace {
constexpr uint32_t kBootAnimLvglLockTimeoutMs = 100;
constexpr uint32_t kBootAnimFinishLvglLockTimeoutMs = 200;
constexpr uint32_t kBootAnimFinishHoldMs = 100;
constexpr uint32_t kBootScreenLvglLockTimeoutMs = 2000;
constexpr int kBootContentX = 28;
constexpr int kBootContentW = 344;
constexpr int kBootTitleY = 30;
constexpr int kBootTitleH = 30;
constexpr int kBootStatusY = 64;
constexpr int kBootStatusH = 24;
constexpr int kBootVersionY = 226;
constexpr int kBootVersionH = 24;
constexpr int kBootDetailY = 256;
constexpr int kBootDetailH = 22;
constexpr int kBootAnimCanvasX = 144;
constexpr int kBootAnimCanvasY = 100;
constexpr const char *kBootTitleText = "RLCD Weather Clock";
constexpr const char *kBootInitialStatusText = "Starting...";
constexpr const char *kBootInitialDetailText = "Preparing system";
#define BOOT_ANIM_DONE_EVENT_SKIPPED_LOG "boot anim done event skipped: app events unavailable"
#define BOOT_ANIM_CANVAS_CREATE_FAILED_LOG "boot anim canvas create failed"
lv_color_t *s_boot_anim_canvas_buffer;
lv_obj_t *s_boot_status_label;
lv_obj_t *s_boot_detail_label;
lv_obj_t *s_boot_anim_canvas;
std::atomic<bool> s_boot_anim_running{false};

static_assert(kBootContentX >= 0 && kBootContentW > 0,
              "boot screen content frame must be valid");
static_assert(kBootTitleH > 0 && kBootStatusH > 0 && kBootVersionH > 0 && kBootDetailH > 0,
              "boot screen label heights must be positive");
static_assert(kBootAnimCanvasX >= 0 && kBootAnimCanvasY >= 0,
              "boot animation canvas position must be non-negative");
static_assert(kBootAnimLvglLockTimeoutMs > 0, "boot animation LVGL lock timeout must be positive");
static_assert(kBootAnimFinishLvglLockTimeoutMs >= kBootAnimLvglLockTimeoutMs,
              "boot animation finish lock timeout must cover the normal lock timeout");
static_assert(kBootScreenLvglLockTimeoutMs >= kBootAnimFinishLvglLockTimeoutMs,
              "boot screen lock timeout must cover boot animation finish lock timeout");
void release_boot_anim_canvas_buffer()
{
    std::free(s_boot_anim_canvas_buffer);
    s_boot_anim_canvas_buffer = nullptr;
}
} // namespace

static void draw_boot_anim_frame_index(int frame)
{
    if (!s_boot_anim_canvas || !s_boot_anim_canvas_buffer) {
        return;
    }
    if (frame < 0) {
        frame = 0;
    } else if (frame >= BOOT_ANIM_FRAME_COUNT) {
        frame = BOOT_ANIM_FRAME_COUNT - 1;
    }
    lv_img_dsc_t *image = lv_canvas_get_img(s_boot_anim_canvas);
    if (!image) {
        return;
    }
    const uint8_t *pixels = boot_anim_frames[frame];
    uint32_t bit = 0;
    for (int y = 0; y < BOOT_ANIM_HEIGHT; ++y) {
        for (int x = 0; x < BOOT_ANIM_WIDTH; ++x, ++bit) {
            bool black = packed_1bit_bit_is_set(pixels, bit);
            lv_img_buf_set_px_color(image, x, y, black ? lv_color_black() : lv_color_white());
        }
    }
    lv_obj_invalidate(s_boot_anim_canvas);
}

void prepare_boot_animation()
{
    s_boot_anim_running.store(true, std::memory_order_release);
}

void request_boot_animation_stop()
{
    s_boot_anim_running.store(false, std::memory_order_release);
}

void boot_anim_task(void *)
{
    int frame = 0;
    while (s_boot_anim_running.load(std::memory_order_acquire)) {
        if (Lvgl_lock(kBootAnimLvglLockTimeoutMs)) {
            draw_boot_anim_frame_index(frame);
            lv_refr_now(nullptr);
            Lvgl_unlock();
        }
        frame = (frame + 1) % BOOT_ANIM_FRAME_COUNT;
        vTaskDelay(pdMS_TO_TICKS(kBootAnimRunFrameMs));
    }
    if (app_event_group_ready()) {
        app_event_group_set_bits(kBootAnimDoneBit);
    } else {
        ESP_LOGW(TAG, BOOT_ANIM_DONE_EVENT_SKIPPED_LOG);
    }
    vTaskDelete(nullptr);
}

void finish_boot_anim_to_last_frame()
{
    if (Lvgl_lock(kBootAnimFinishLvglLockTimeoutMs)) {
        draw_boot_anim_frame_index(BOOT_ANIM_FRAME_COUNT - 1);
        lv_refr_now(nullptr);
        Lvgl_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(kBootAnimFinishHoldMs));
}

void show_boot_screen()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    make_centered_label_with_font(screen, kBootContentX, kBootTitleY, kBootContentW, kBootTitleH,
                                  kBootTitleText, &lv_font_montserrat_16, "boot title create failed");
    s_boot_status_label = make_centered_label_with_font(screen, kBootContentX, kBootStatusY, kBootContentW,
                                                        kBootStatusH, kBootInitialStatusText,
                                                        &lv_font_montserrat_16, "boot status label create failed");
    s_boot_detail_label = make_centered_label_with_font(screen, kBootContentX, kBootDetailY, kBootContentW,
                                                        kBootDetailH, kBootInitialDetailText,
                                                        &lv_font_montserrat_14, "boot detail label create failed");
    make_centered_label_with_font(screen, kBootContentX, kBootVersionY, kBootContentW, kBootVersionH,
                                  APP_VERSION, &lv_font_montserrat_16, "boot version label create failed");

    if (!s_boot_anim_canvas_buffer) {
        s_boot_anim_canvas_buffer = alloc_canvas_buffer(BOOT_ANIM_WIDTH, BOOT_ANIM_HEIGHT);
    }
    s_boot_anim_canvas = nullptr;
    if (s_boot_anim_canvas_buffer) {
        s_boot_anim_canvas = lv_canvas_create(screen);
        if (!s_boot_anim_canvas) {
            ESP_LOGW(TAG, BOOT_ANIM_CANVAS_CREATE_FAILED_LOG);
        }
    }
    if (s_boot_anim_canvas) {
        configure_canvas_base(s_boot_anim_canvas,
                              s_boot_anim_canvas_buffer,
                              kBootAnimCanvasX,
                              kBootAnimCanvasY,
                              BOOT_ANIM_WIDTH,
                              BOOT_ANIM_HEIGHT);
        lv_canvas_fill_bg(s_boot_anim_canvas, lv_color_white(), LV_OPA_COVER);
        draw_boot_anim_frame_index(0);
    }
    lv_refr_now(nullptr);
}

void update_boot_screen(int percent, const char *status, const char *detail)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    if (Lvgl_lock(kBootScreenLvglLockTimeoutMs)) {
        if (s_boot_status_label) {
            set_label_text_if_changed(s_boot_status_label, status);
        }
        if (s_boot_detail_label) {
            set_label_text_if_changed(s_boot_detail_label, detail);
        }
        lv_refr_now(nullptr);
        Lvgl_unlock();
    }
}

bool finish_boot_screen()
{
    if (!Lvgl_lock(kBootScreenLvglLockTimeoutMs)) {
        return false;
    }
    lv_obj_clean(lv_scr_act());
    clear_clock_object_refs();
    clear_info_object_refs();
    s_boot_status_label = nullptr;
    s_boot_detail_label = nullptr;
    s_boot_anim_canvas = nullptr;
    release_boot_anim_canvas_buffer();
    active_work_page_store(first_enabled_work_page());
    show_active_work_page();
    lv_refr_now(nullptr);
    Lvgl_unlock();
    return true;
}
