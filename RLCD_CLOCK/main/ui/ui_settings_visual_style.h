// 设置页测试皮肤：仅改变视觉属性，菜单结构与输入状态不变。
#pragma once
#include "lvgl.h"

inline void settings_attach_category_marker(lv_obj_t *label) {
    if(!label) return;
    lv_obj_add_event_cb(label,[](lv_event_t *e) {
        lv_obj_t *obj=lv_event_get_target(e);
        const int width=lv_obj_get_style_line_width(obj,0);
        if(width==0) return;
        lv_area_t area; lv_obj_get_coords(obj,&area);
        lv_area_t marker={(lv_coord_t)(area.x1+4),(lv_coord_t)(area.y1+6),
                          (lv_coord_t)(area.x1+3+width),(lv_coord_t)(area.y2-6)};
        lv_draw_rect_dsc_t style; lv_draw_rect_dsc_init(&style);
        style.bg_color=lv_color_black();
        lv_draw_rect(lv_event_get_draw_ctx(e),&style,&marker);
    },LV_EVENT_DRAW_MAIN,nullptr);
}

inline void settings_visual_style(lv_obj_t *label,bool selected,bool primary) {
    if(!label) return;
    const bool reverse=selected && !primary;
    lv_obj_set_style_bg_color(label,reverse?lv_color_black():lv_color_white(),0);
    lv_obj_set_style_bg_opa(label,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(label,reverse?lv_color_white():lv_color_black(),0);
    lv_obj_set_style_border_color(label,lv_color_black(),0);
    lv_obj_set_style_border_side(label,LV_BORDER_SIDE_FULL,0);
    lv_obj_set_style_border_width(label,1,0);
    lv_obj_set_style_radius(label,4,0);
    lv_obj_set_style_line_width(label,primary && selected?3:0,0);
    // Center-aligned text moves two pixels; the option frame stays fixed.
    lv_obj_set_style_pad_left(label,primary && selected?14:10,0);
    lv_obj_set_style_pad_right(label,10,0);
    lv_obj_set_style_pad_top(label,5,0);
    lv_obj_set_style_pad_bottom(label,5,0);
}
