// 图片时钟固件与SDL共用随每日文字宽度变化的双线。
#pragma once
#include "lvgl.h"
#include "ui_gallery_layout.h"

inline void gallery_attach_saying_rules(lv_obj_t *label) {
    lv_obj_add_event_cb(label,[](lv_event_t *e) {
        lv_obj_t *obj=lv_event_get_target(e);
        const char *text=lv_label_get_text(obj);
        if(!text || !text[0]) return;
        lv_draw_label_dsc_t text_style; lv_draw_label_dsc_init(&text_style);
        lv_obj_init_draw_label_dsc(obj,LV_PART_MAIN,&text_style);
        lv_point_t size;
        lv_txt_get_size(&size,text,text_style.font,text_style.letter_space,
                        text_style.line_space,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        lv_area_t area; lv_obj_get_coords(obj,&area);
        const int length=ui_gallery_layout::gallery_saying_rule_length(lv_area_get_width(&area),size.x);
        if(length<4 || size.y>text_style.font->line_height) return;
        lv_draw_rect_dsc_t style; lv_draw_rect_dsc_init(&style);
        style.bg_color=text_style.color;
        for(int line=0;line<2;++line) {
            const int y=area.y1+text_style.font->line_height/2-2+line*4;
            lv_area_t left={area.x1,(lv_coord_t)y,(lv_coord_t)(area.x1+length-1),(lv_coord_t)y};
            lv_area_t right={(lv_coord_t)(area.x2-length+1),(lv_coord_t)y,area.x2,(lv_coord_t)y};
            lv_draw_rect(lv_event_get_draw_ctx(e),&style,&left);
            lv_draw_rect(lv_event_get_draw_ctx(e),&style,&right);
        }
    },LV_EVENT_DRAW_MAIN,nullptr);
}
