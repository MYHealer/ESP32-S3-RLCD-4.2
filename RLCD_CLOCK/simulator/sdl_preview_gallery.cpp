// 实现 SDL 图片时钟主体，集中维护图库画布和块状数字预览。
#include "sdl_preview_gallery.h"

#include "clock_gallery_images.h"
#include "sdl_preview_widgets.h"
#include "ui_gallery_layout.h"
#include "ui_gallery_decoration.h"

#include <vector>

LV_FONT_DECLARE(zh_font_16);

namespace {
using namespace ui_gallery_layout;

using sdl_preview_widgets::canvas_fill_rect;
using sdl_preview_widgets::draw_1bit_icon;
using sdl_preview_widgets::make_black_bar;
using sdl_preview_widgets::make_label;

constexpr const char *kSayingPreview = "今日无事，适合慢慢来。";

std::vector<lv_color_t> g_gallery_image_canvas_pixels(
    CLOCK_GALLERY_IMAGE_WIDTH * CLOCK_GALLERY_IMAGE_HEIGHT);
std::vector<lv_color_t> g_gallery_time_canvas_pixels(
    kGalleryTimeCanvasW * kGalleryTimeCanvasH);

void draw_block_digit(lv_obj_t *canvas, int digit, int x, int y)
{
    if (digit < 0 || digit >= kGalleryBlockDigitCount) {
        return;
    }
    for (int row = 0; row < kGalleryBlockDigitRows; ++row) {
        for (int col = 0; col < kGalleryBlockDigitCols; ++col) {
            if (kGalleryBlockDigits[digit][row][col] == '1') {
                canvas_fill_rect(canvas,
                                 x + col * kGalleryBlockDigitScale,
                                 y + row * kGalleryBlockDigitScale,
                                 kGalleryBlockDigitScale - 1,
                                 kGalleryBlockDigitScale - 1,
                                 lv_color_black());
            }
        }
    }
}

void draw_block_number(lv_obj_t *canvas, int value, int y)
{
    draw_block_digit(canvas,
                     value / kGalleryDecimalBase,
                     gallery_block_digit_x(0),
                     y);
    draw_block_digit(canvas,
                     value % kGalleryDecimalBase,
                     gallery_block_digit_x(1),
                     y);
}
} // namespace

void build_gallery_preview_body(lv_obj_t *screen, const struct tm *local)
{
    if (!screen || !local) {
        return;
    }

    lv_obj_t *image_canvas = lv_canvas_create(screen);
    lv_obj_clear_flag(image_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(image_canvas,
                   kGalleryImageCanvasX,
                   kGalleryImageCanvasY);
    lv_obj_set_size(image_canvas, CLOCK_GALLERY_IMAGE_WIDTH, CLOCK_GALLERY_IMAGE_HEIGHT);
    lv_obj_set_style_border_width(image_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(image_canvas, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(image_canvas,
                         g_gallery_image_canvas_pixels.data(),
                         CLOCK_GALLERY_IMAGE_WIDTH,
                         CLOCK_GALLERY_IMAGE_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    draw_1bit_icon(image_canvas,
                   CLOCK_GALLERY_IMAGE_WIDTH,
                   CLOCK_GALLERY_IMAGE_HEIGHT,
                   CLOCK_GALLERY_IMAGE_BYTES_PER_ROW,
                   clock_gallery_images[local->tm_wday % CLOCK_GALLERY_IMAGE_COUNT],
                   lv_color_black(),
                   lv_color_white());

    make_black_bar(screen,
                   kGalleryDividerX,
                   kGalleryDividerY,
                   kGalleryDividerW,
                   kGalleryDividerH);

    lv_obj_t *time_canvas = lv_canvas_create(screen);
    lv_obj_clear_flag(time_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(time_canvas,
                   kGalleryTimeCanvasX,
                   kGalleryTimeCanvasY);
    lv_obj_set_size(time_canvas,
                    kGalleryTimeCanvasW,
                    kGalleryTimeCanvasH);
    lv_obj_set_style_border_width(time_canvas, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(time_canvas, 0, LV_PART_MAIN);
    lv_canvas_set_buffer(time_canvas,
                         g_gallery_time_canvas_pixels.data(),
                         kGalleryTimeCanvasW,
                         kGalleryTimeCanvasH,
                         LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(time_canvas, lv_color_white(), LV_OPA_COVER);
    draw_block_number(time_canvas, local->tm_hour, kGalleryTimeHourY);
    draw_block_number(time_canvas, local->tm_min, kGalleryTimeMinuteY);
    lv_obj_invalidate(time_canvas);

    lv_obj_t *saying = make_label(screen,
                                  kGallerySayingLabelX,
                                  kGallerySayingLabelY,
                                  kGallerySayingLabelW,
                                  kGallerySayingLabelH,
                                  kSayingPreview);
    lv_obj_set_style_text_font(saying, &zh_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(saying, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(saying, LV_LABEL_LONG_DOT);
    gallery_attach_saying_rules(saying);
}
