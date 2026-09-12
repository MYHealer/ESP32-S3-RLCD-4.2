/* 小智番茄钟标题专用 24px 中文子集字体，仅包含“番茄钟”三个字。 */
/*******************************************************************************
 * Size: 24 px
 * Bpp: 1
 * Opts: --font <font-file> --symbols 番茄钟 --size 24 --bpp 1 --format lvgl --lv-include lvgl.h --lv-font-name zh_pomodoro_title_24 --lv-fallback lv_font_montserrat_14 --output RLCD_CLOCK/main/assets/zh_pomodoro_title_24.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef ZH_POMODORO_TITLE_24
#define ZH_POMODORO_TITLE_24 1
#endif

#if ZH_POMODORO_TITLE_24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+756A "番" */
    0x0, 0x0, 0x20, 0x0, 0x3f, 0xc3, 0xff, 0xf0,
    0x7, 0x8c, 0x30, 0x6, 0x31, 0x80, 0xc, 0xdc,
    0x7, 0xff, 0xff, 0x9f, 0xff, 0xfe, 0x1, 0xff,
    0x0, 0xe, 0xdf, 0x1, 0xf3, 0x1f, 0x3f, 0x80,
    0x3f, 0x7f, 0xff, 0xd8, 0x30, 0xc3, 0x0, 0xc3,
    0xc, 0x3, 0xff, 0xf0, 0xf, 0xff, 0xc0, 0x30,
    0xc3, 0x0, 0xc3, 0xc, 0x3, 0xff, 0xf0, 0xc,
    0x0, 0xc0, 0x30, 0x3, 0x0,

    /* U+8304 "茄" */
    0x3, 0x1, 0x80, 0xc, 0x6, 0x0, 0x30, 0x18,
    0x1f, 0xff, 0xff, 0x7f, 0xff, 0xfc, 0xc, 0x6,
    0x0, 0x30, 0x18, 0x1, 0x80, 0x60, 0x6, 0x0,
    0x0, 0x18, 0x1f, 0xe3, 0xff, 0x7f, 0x8f, 0xfd,
    0x86, 0x6, 0x36, 0x18, 0x18, 0xd8, 0x60, 0x63,
    0x61, 0x81, 0x8d, 0x86, 0x6, 0x36, 0x18, 0x30,
    0xd8, 0x60, 0xc3, 0x61, 0x86, 0xd, 0xfe, 0x73,
    0xe6, 0x19, 0x8f, 0x98, 0x60,

    /* U+949F "钟" */
    0xc, 0x1, 0x80, 0x60, 0xc, 0x3, 0xf0, 0x60,
    0x3f, 0x83, 0x1, 0x83, 0xff, 0x98, 0x1f, 0xfc,
    0xff, 0xc6, 0x6f, 0xfe, 0x33, 0xe6, 0x31, 0x9a,
    0x31, 0x8c, 0xdf, 0xfc, 0x66, 0xff, 0xff, 0xf0,
    0x63, 0xff, 0x83, 0x18, 0xcc, 0x18, 0x6, 0x0,
    0xc0, 0x30, 0x6, 0xc1, 0x80, 0x3e, 0xc, 0x3,
    0xc0, 0x60, 0x8, 0x3, 0x0, 0x0, 0x18, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 384, .box_w = 22, .box_h = 22, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 61, .adv_w = 384, .box_w = 22, .box_h = 22, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 122, .adv_w = 384, .box_w = 21, .box_h = 21, .ofs_x = 1, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xd9a, 0x1f35
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 30058, .range_length = 7990, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 3, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};

extern const lv_font_t lv_font_montserrat_14;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t zh_pomodoro_title_24 = {
#else
lv_font_t zh_pomodoro_title_24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 23,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_14,
#endif
    .user_data = NULL,
};



#endif /*#if ZH_POMODORO_TITLE_24*/
