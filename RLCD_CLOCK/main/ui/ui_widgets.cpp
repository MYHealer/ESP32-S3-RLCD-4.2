// 创建并更新项目共用的 LVGL 条形块和文本标签。
#include "ui_widgets.h"

#include "app_metadata.h"
#include "ui_fonts.h"

#include <esp_log.h>
#include <string.h>

#define UI_BAR_PARENT_UNAVAILABLE_LOG "bar parent unavailable"
#define UI_BAR_INVALID_SIZE_FORMAT "bar invalid size %dx%d"
#define UI_BAR_CREATE_FAILED_LOG "bar create failed"
#define UI_LABEL_PARENT_UNAVAILABLE_LOG "label parent unavailable"
#define UI_LABEL_INVALID_SIZE_FORMAT "label invalid size %dx%d"
#define UI_LABEL_CREATE_FAILED_LOG "label create failed"

namespace {
const char *label_text_or_empty(const char *text)
{
    return text ? text : "";
}

void warn_if_center_align_failed(lv_obj_t *label, const char *warning)
{
    if (!center_align_label(label)) {
        ESP_LOGW(TAG, "%s", warning && warning[0] ? warning : UI_LABEL_CREATE_FAILED_LOG);
    }
}
}

void set_obj_box(lv_obj_t *obj, int x, int y, int w, int h)
{
    if (!obj) {
        return;
    }
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
}

void set_obj_black(lv_obj_t *obj, bool active)
{
    if (!obj) {
        return;
    }
    lv_obj_set_style_bg_color(obj, active ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 1, LV_PART_MAIN);
}

lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h)
{
    if (!parent) {
        ESP_LOGW(TAG, "%s", UI_BAR_PARENT_UNAVAILABLE_LOG);
        return nullptr;
    }
    if (w <= 0 || h <= 0) {
        ESP_LOGW(TAG, UI_BAR_INVALID_SIZE_FORMAT, w, h);
        return nullptr;
    }
    lv_obj_t *bar = lv_obj_create(parent);
    if (!bar) {
        ESP_LOGW(TAG, "%s", UI_BAR_CREATE_FAILED_LOG);
        return nullptr;
    }
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    set_obj_box(bar, x, y, w, h);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    set_obj_black(bar, false);
    return bar;
}

lv_obj_t *make_black_bar(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *bar = make_bar(parent, x, y, w, h);
    set_obj_black(bar, true);
    return bar;
}

lv_obj_t *make_label_with_font(lv_obj_t *parent,
                               int x,
                               int y,
                               int w,
                               int h,
                               const char *text,
                               const lv_font_t *font)
{
    if (!parent) {
        ESP_LOGW(TAG, "%s", UI_LABEL_PARENT_UNAVAILABLE_LOG);
        return nullptr;
    }
    if (w <= 0 || h <= 0) {
        ESP_LOGW(TAG, UI_LABEL_INVALID_SIZE_FORMAT, w, h);
        return nullptr;
    }
    lv_obj_t *label = lv_label_create(parent);
    if (!label) {
        ESP_LOGW(TAG, "%s", UI_LABEL_CREATE_FAILED_LOG);
        return nullptr;
    }
    set_obj_box(label, x, y, w, h);
    lv_label_set_text(label, label_text_or_empty(text));
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    if (font) {
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    }
    lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
    return label;
}

lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w, int h, const char *text)
{
    return make_label_with_font(parent, x, y, w, h, text, &zh_font_16);
}

bool center_align_label(lv_obj_t *label)
{
    if (!label) {
        return false;
    }
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return true;
}

lv_obj_t *make_centered_label(lv_obj_t *parent,
                              int x,
                              int y,
                              int w,
                              int h,
                              const char *text,
                              const char *warning)
{
    lv_obj_t *label = make_label(parent, x, y, w, h, text);
    warn_if_center_align_failed(label, warning);
    return label;
}

lv_obj_t *make_centered_label_with_font(lv_obj_t *parent,
                                        int x,
                                        int y,
                                        int w,
                                        int h,
                                        const char *text,
                                        const lv_font_t *font,
                                        const char *warning)
{
    lv_obj_t *label = make_label_with_font(parent, x, y, w, h, text, font);
    warn_if_center_align_failed(label, warning);
    return label;
}

bool set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if (!label) {
        return false;
    }
    text = label_text_or_empty(text);
    const char *current = lv_label_get_text(label);
    if (current == nullptr || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
        return true;
    }
    return false;
}
