// 声明通用 LVGL 条形块和文本标签的创建、样式与更新接口。
#pragma once

#include "lvgl.h"

void set_obj_box(lv_obj_t *obj, int x, int y, int w, int h);
void set_obj_black(lv_obj_t *obj, bool active);
lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h);
lv_obj_t *make_black_bar(lv_obj_t *parent, int x, int y, int w, int h);
lv_obj_t *make_label_with_font(lv_obj_t *parent,
                               int x,
                               int y,
                               int w,
                               int h,
                               const char *text,
                               const lv_font_t *font);
lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w, int h, const char *text);
bool center_align_label(lv_obj_t *label);
lv_obj_t *make_centered_label(lv_obj_t *parent,
                              int x,
                              int y,
                              int w,
                              int h,
                              const char *text,
                              const char *warning);
lv_obj_t *make_centered_label_with_font(lv_obj_t *parent,
                                        int x,
                                        int y,
                                        int w,
                                        int h,
                                        const char *text,
                                        const lv_font_t *font,
                                        const char *warning);
bool set_label_text_if_changed(lv_obj_t *label, const char *text);
