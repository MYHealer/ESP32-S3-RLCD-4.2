// 构建并刷新 MiPlay 音乐播放工作页。
//
// 布局参考 xiaozhi-esp32 music_ui.cc，适配 400x300 monochrome RLCD（白底黑字）。
//
// ┌──────────────────────────────────────────┐
// │ [状态栏 + 分隔线]                         │  y=0..58
// │                                          │
// │  ┌──────────┐   歌曲标题                  │
// │  │          │   歌手名                    │
// │  │  ◉ 唱片  │   ─────────────            │
// │  │ (120x120)│   上一句歌词（灰色）         │  y=62
// │  │          │   当前歌词（醒目黑色）       │
// │  │          │   下一句歌词（灰色）         │
// │  └──────────┘                            │
// │                                          │
// │  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  02:31    │  进度条 + 时间
// │                                          │
// │         > 播放中                          │  播放状态
// │                                          │
// │              等待投放...                   │  空闲占位
// └──────────────────────────────────────────┘
//
#include "ui_miplay_player.h"
#include "ui_views.h"

#include "app_display_config.h"
#include "ui_battery.h"
#include "ui_fonts.h"
#include "ui_work_page_layout.h"
#include "ui_page_state.h"
#include "miplay_media_state.h"
#include "lyric_bridge.h"
#include "spectrum.h"

#include <esp_attr.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

namespace {
constexpr char kTag[] = "ui_miplay_player";

// ── 布局常量 ──
constexpr int kPad = 12;

// 唱片卡（左侧）
constexpr int kVinylCardSize = 120;
constexpr int kVinylSize = 100;
constexpr int kVinylX = kPad;
constexpr int kVinylY = 74;
constexpr int kVinylCenterSize = 32;
constexpr int kVinylHoleSize = 8;

// 歌曲信息卡（右侧）
constexpr int kInfoX = kVinylX + kVinylCardSize + 10;
constexpr int kInfoW = kDisplayWidth - kInfoX - kPad;
constexpr int kInfoY = kVinylY;
constexpr int kInfoH = kVinylCardSize;

// 歌词区域（信息卡内）
constexpr int kLyricSepY = 48;      // 分隔线在信息卡内的 y
constexpr int kLyricStartY = 54;    // 歌词起始 y（信息卡内）
constexpr int kLyricLineH = 20;     // 每行歌词高度
constexpr int kLyricGap = 2;        // 行间距

// 进度条（左=当前时间，中=进度条，右=总时长）
constexpr int kBarY = kVinylY + kVinylCardSize + 12;
constexpr int kTimeEstW = 34;     // "0:00" ~4字符×8px 估算宽度
constexpr int kBarX = kPad + kTimeEstW + 4;
constexpr int kBarW = kDisplayWidth - kPad * 2 - kTimeEstW * 2 - 12;
constexpr int kBarH = 10;
constexpr int kProgressMax = 1000;

// 当前时间（进度条左侧）
constexpr int kCurTimeX = kPad;
constexpr int kCurTimeY = kBarY - 2;

// 总时长（进度条右侧）
constexpr int kTotalTimeX = kBarX + kBarW + 4;
constexpr int kTotalTimeY = kBarY - 2;

// 控制按钮（进度条下方居中）
constexpr int kBtnY = kBarY + 20;
constexpr int kBtnSize = 36;
constexpr int kBtnGap = 24;
constexpr int kBtnAreaW = kBtnSize * 3 + kBtnGap * 2;
constexpr int kBtnStartX = (kDisplayWidth - kBtnAreaW) / 2;
constexpr int kBtnPlayX = kBtnStartX + kBtnSize + kBtnGap;
constexpr int kBtnNextX = kBtnStartX + (kBtnSize + kBtnGap) * 2;

// 刷新间隔
constexpr uint32_t kPlayingRefreshMs = 500;
constexpr uint32_t kIdleRefreshMs = 2000;

// 频谱条（按钮下方）
constexpr int kSpecBarCount = 12;
constexpr int kSpecBarW = 26;
constexpr int kSpecBarGap = 6;
constexpr int kSpecBarMaxH = 36;
constexpr int kSpecAreaW = kSpecBarCount * kSpecBarW + (kSpecBarCount - 1) * kSpecBarGap;
constexpr int kSpecStartX = (kDisplayWidth - kSpecAreaW) / 2;
constexpr int kSpecY = kBtnY + kBtnSize + 6;

// LVGL 对象引用
lv_obj_t *s_screen = nullptr;  // 用于在 update 中创建绝对定位的图标
lv_obj_t *s_vinyl_card = nullptr;
lv_obj_t *s_title_label = nullptr;
lv_obj_t *s_artist_label = nullptr;
lv_obj_t *s_lyric_prev_label = nullptr;
lv_obj_t *s_lyric_label = nullptr;
lv_obj_t *s_lyric_next_label = nullptr;
lv_obj_t *s_progress_bar = nullptr;
lv_obj_t *s_cur_time_label = nullptr;   // 进度条左侧：当前时间
lv_obj_t *s_total_time_label = nullptr; // 进度条右侧：总时长
lv_obj_t *s_btn_prev = nullptr;
lv_obj_t *s_btn_play = nullptr;
lv_obj_t *s_btn_next = nullptr;
lv_obj_t *s_play_icon_lines[4] = {};    // 播放/暂停图标线段（最多4条）
bool s_play_icon_is_pause = false;       // 当前图标是否为暂停
lv_obj_t *s_idle_label = nullptr;
lv_obj_t *s_spec_bars[kSpecBarCount] = {};   // 频谱条
lv_obj_t *s_spec_peaks[kSpecBarCount] = {};  // peak hold 指示线

// CJK fallback 字体链（共享实现，见 ui_fonts_shared.cpp）
static const lv_font_t *get_cjk_font()
{
    return get_cjk_font_with_supplement();
}

// 上次更新的值
int s_last_lyric_index = -1;
bool s_last_playing = false;
bool s_last_paused = false;
int64_t s_last_position_ms = -1;
int64_t s_last_duration_ms = -1;
uint32_t s_last_meta_version = 0;
uint32_t s_last_lyric_version = 0;
int s_last_progress = -1;

// ── 辅助函数 ──

void format_time(char *buf, size_t len, int64_t ms)
{
    if (ms < 0) ms = 0;
    int total_sec = static_cast<int>(ms / 1000);
    int min = total_sec / 60;
    int sec = total_sec % 60;
    snprintf(buf, len, "%d:%02d", min, sec);
}

// 创建圆角白色卡片
lv_obj_t *create_rounded_card(lv_obj_t *parent, int x, int y, int w, int h, int radius)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_radius(card, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

// 创建圆形对象
lv_obj_t *create_circle(lv_obj_t *parent, int size, lv_color_t bg, lv_color_t border, int border_w)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(c, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, border_w, LV_PART_MAIN);
    lv_obj_set_style_border_color(c, border, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

// ── 用 lv_canvas 画填充三角形 ──
// 返回 canvas 对象，定位在 (abs_cx - half, abs_cy - half)
// dir: -1=左, +1=右
static lv_obj_t *draw_triangle(lv_obj_t *parent, int abs_cx, int abs_cy, int size, int dir)
{
    int half = size / 2;
    int buf_sz = LV_CANVAS_BUF_SIZE_INDEXED_1BIT(size, size);
    void *buf = lv_mem_alloc(buf_sz);
    if (!buf) return nullptr;

    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buf, size, size, LV_IMG_CF_INDEXED_1BIT);
    // 调色板: 0=透明背景, 1=黑
    lv_canvas_set_palette(canvas, 0, lv_color_white());
    lv_canvas_set_palette(canvas, 1, lv_color_black());

    // 清为背景色
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_TRANSP);

    // 填充三角形: 逐扫描线光栅化
    // 左(dir<0): tip=(0,half), base=(size,0)-(size,size)
    // 右(dir>0): tip=(size,half), base=(0,0)-(0,size)
    for (int y = 0; y < size; y++) {
        // x 从 tip 到 base 边插值
        // tip_x = (dir>0) ? size : 0
        // base_x = (dir>0) ? 0 : size
        // x = tip_x + (base_x - tip_x) * abs(y - half) / half
        int tip_x = (dir > 0) ? size : 0;
        int base_x = (dir > 0) ? 0 : size;
        int dist = (y < half) ? (half - y) : (y - half);
        int x = tip_x + (base_x - tip_x) * dist / half;
        int x_start = (x < tip_x) ? x : tip_x;
        int x_end = (x > tip_x) ? x : tip_x;
        for (int px = x_start; px <= x_end; px++) {
            lv_canvas_set_px_color(canvas, px, y, lv_color_black());
        }
    }

    // 定位: canvas 左上角 = (abs_cx - half, abs_cy - half)
    lv_obj_set_pos(canvas, abs_cx - half, abs_cy - half);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, LV_PART_MAIN);
    return canvas;
}

// 创建传输按钮（纯容器 + 点击事件，无默认按钮样式干扰）
static lv_obj_t *create_transport_btn(lv_obj_t *parent, int x, int y, int size,
                                       lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    // 按压时无视觉变化
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    return btn;
}

// ── 更新播放/暂停图标 ──
// 图标直接画在 screen 上用绝对坐标，避免 button 子对象的布局偏移
static void update_play_icon(bool is_pause)
{
    if (s_play_icon_is_pause == is_pause && s_play_icon_lines[0]) return;
    s_play_icon_is_pause = is_pause;

    // 清除旧图标
    for (int i = 0; i < 4; i++) {
        if (s_play_icon_lines[i]) {
            lv_obj_del(s_play_icon_lines[i]);
            s_play_icon_lines[i] = nullptr;
        }
    }

    if (!s_btn_play || !s_screen) return;

    // 播放按钮在 screen 上的绝对中心坐标
    int abs_cx = kBtnPlayX + kBtnSize / 2;
    int abs_cy = kBtnY + kBtnSize / 2;

    if (is_pause) {
        // 暂停图标：两个竖条，画在 screen 上
        for (int i = 0; i < 2; i++) {
            int x_off = (i == 0) ? -6 : 6;
            lv_obj_t *bar = lv_obj_create(s_screen);
            lv_obj_set_size(bar, 3, 16);
            lv_obj_set_pos(bar, abs_cx + x_off - 1, abs_cy - 8);
            lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(bar, 1, LV_PART_MAIN);
            lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
            s_play_icon_lines[i] = bar;
        }
    } else {
        // 播放图标：右三角
        s_play_icon_lines[0] = draw_triangle(s_screen, abs_cx, abs_cy, 14, 1);
    }
}

} // namespace

// ── 传输控制（external linkage，供 input_tasks.cpp 调用）──
static MiPlayControlFn s_prev_fn = nullptr;
static MiPlayControlFn s_play_pause_fn = nullptr;
static MiPlayControlFn s_next_fn = nullptr;

void miplay_register_control_callbacks(MiPlayControlFn prev,
                                        MiPlayControlFn play_pause,
                                        MiPlayControlFn next)
{
    s_prev_fn = prev;
    s_play_pause_fn = play_pause;
    s_next_fn = next;
}

void miplay_trigger_prev(void) { if (s_prev_fn) s_prev_fn(); }
void miplay_trigger_play_pause(void) { if (s_play_pause_fn) s_play_pause_fn(); }
void miplay_trigger_next(void) { if (s_next_fn) s_next_fn(); }

static void btn_prev_cb(lv_event_t *e) { (void)e; miplay_trigger_prev(); }
static void btn_play_cb(lv_event_t *e) { (void)e; miplay_trigger_play_pause(); }
static void btn_next_cb(lv_event_t *e) { (void)e; miplay_trigger_next(); }

void build_miplay_player_page()
{
    lv_obj_t *screen = work_page_root(kWorkPageMiPlayPlayer);
    if (screen) {
        return;
    }

    screen = create_page_root();
    if (!screen) {
        return;
    }
    s_screen = screen;
    set_work_page_root(kWorkPageMiPlayPlayer, screen);

    build_work_page_battery_icon(screen, kWorkPageMiPlayPlayer);
    build_work_page_status_bar(screen, kWorkPageMiPlayPlayer, false, true);

    // 分隔线
    lv_obj_t *sep = lv_obj_create(screen);
    lv_obj_set_pos(sep, ui_work_page_layout::kTopSeparatorX,
                   ui_work_page_layout::kTopSeparatorY);
    lv_obj_set_size(sep, ui_work_page_layout::kTopSeparatorWidth,
                    ui_work_page_layout::kTopSeparatorHeight);
    lv_obj_set_style_bg_color(sep, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sep, 2, LV_PART_MAIN);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // ══════════════════════════════════════════
    // 左侧：唱片卡片（白底圆角 + 黑色圆盘 + 同心圆纹路 + 中心白圆 + 小孔）
    // ══════════════════════════════════════════

    s_vinyl_card = create_rounded_card(screen, kVinylX, kVinylY,
                                       kVinylCardSize, kVinylCardSize, 16);

    // 黑色圆盘
    lv_obj_t *disc = create_circle(s_vinyl_card, kVinylSize,
                                   lv_color_black(), lv_color_white(), 2);
    lv_obj_center(disc);

    // 同心圆纹路（3 层半透明白色边框）
    const int ring_sizes[] = {80, 64, 48};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ring = lv_obj_create(disc);
        lv_obj_set_size(ring, ring_sizes[i], ring_sizes[i]);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(ring, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ring, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_opa(ring, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_pad_all(ring, 0, LV_PART_MAIN);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    // 中心白色圆
    lv_obj_t *center = create_circle(disc, kVinylCenterSize,
                                     lv_color_white(), lv_color_black(), 2);
    lv_obj_center(center);

    // 中心小孔
    lv_obj_t *hole = create_circle(center, kVinylHoleSize,
                                   lv_color_black(), lv_color_black(), 0);
    lv_obj_center(hole);

    // ══════════════════════════════════════════
    // 右侧：歌曲信息卡（标题 + 歌手 + 分隔线 + 3行歌词）
    // ══════════════════════════════════════════

    lv_obj_t *info_card = create_rounded_card(screen, kInfoX, kInfoY,
                                              kInfoW, kInfoH, 16);
    lv_obj_set_style_pad_left(info_card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(info_card, 10, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(info_card, true, LV_PART_MAIN);

    // 歌名（长名自动滚动）
    s_title_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(s_title_label, get_cjk_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_title_label, kInfoW - 20);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_title_label, "");
    lv_obj_align(s_title_label, LV_ALIGN_TOP_LEFT, 0, 4);

    // 歌手（小字低透明度）
    s_artist_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(s_artist_label, get_cjk_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_artist_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_artist_label, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_artist_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_artist_label, kInfoW - 20);
    lv_label_set_long_mode(s_artist_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_artist_label, "");
    lv_obj_align(s_artist_label, LV_ALIGN_TOP_LEFT, 0, 24);

    // 分隔线
    lv_obj_t *info_sep = lv_obj_create(info_card);
    lv_obj_set_size(info_sep, kInfoW - 24, 1);
    lv_obj_set_style_bg_color(info_sep, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(info_sep, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_border_width(info_sep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(info_sep, 0, LV_PART_MAIN);
    lv_obj_align(info_sep, LV_ALIGN_TOP_LEFT, 0, kLyricSepY);
    lv_obj_clear_flag(info_sep, LV_OBJ_FLAG_SCROLLABLE);

    // 上一句歌词（小字，灰色，单行省略号截断）
    s_lyric_prev_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(s_lyric_prev_label, get_cjk_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lyric_prev_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_lyric_prev_label, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lyric_prev_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_size(s_lyric_prev_label, kInfoW - 20, kLyricLineH);
    lv_label_set_long_mode(s_lyric_prev_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lyric_prev_label, "");
    lv_obj_align(s_lyric_prev_label, LV_ALIGN_TOP_LEFT, 0, kLyricStartY);

    // 当前歌词（醒目黑色，长行自动滚动）
    s_lyric_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(s_lyric_label, get_cjk_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lyric_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lyric_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_lyric_label, kInfoW - 20);
    lv_label_set_long_mode(s_lyric_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(s_lyric_label, "");
    lv_obj_align(s_lyric_label, LV_ALIGN_TOP_LEFT, 0,
                 kLyricStartY + kLyricLineH + kLyricGap);

    // 下一句歌词（小字，灰色，单行省略号截断）
    s_lyric_next_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(s_lyric_next_label, get_cjk_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lyric_next_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_lyric_next_label, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lyric_next_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_size(s_lyric_next_label, kInfoW - 20, kLyricLineH);
    lv_label_set_long_mode(s_lyric_next_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lyric_next_label, "");
    lv_obj_align(s_lyric_next_label, LV_ALIGN_TOP_LEFT, 0,
                 kLyricStartY + (kLyricLineH + kLyricGap) * 2);

    // ══════════════════════════════════════════
    // 进度条 + 当前时间/总时长 + 控制按钮
    // ══════════════════════════════════════════

    // 当前时间（进度条左侧）
    s_cur_time_label = lv_label_create(screen);
    lv_obj_set_pos(s_cur_time_label, kCurTimeX, kCurTimeY);
    lv_obj_set_style_text_font(s_cur_time_label, &zh_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cur_time_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_cur_time_label, LV_OPA_70, LV_PART_MAIN);
    lv_label_set_text(s_cur_time_label, "0:00");

    // 进度条
    s_progress_bar = lv_bar_create(screen);
    lv_obj_set_pos(s_progress_bar, kBarX, kBarY);
    lv_obj_set_size(s_progress_bar, kBarW, kBarH);
    lv_bar_set_range(s_progress_bar, 0, kProgressMax);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);

    // 轨道样式：灰底 + 黑边 + 圆角
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_progress_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_progress_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_progress_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_progress_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_progress_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_progress_bar, 2, LV_PART_MAIN);

    // 指示器：纯黑填充
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress_bar, 3, LV_PART_INDICATOR);

    // 总时长（进度条右侧）
    s_total_time_label = lv_label_create(screen);
    lv_obj_set_pos(s_total_time_label, kTotalTimeX, kTotalTimeY);
    lv_obj_set_style_text_font(s_total_time_label, &zh_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_total_time_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_total_time_label, LV_OPA_70, LV_PART_MAIN);
    lv_label_set_text(s_total_time_label, "0:00");

    // 控制按钮（上一首 / 播放暂停 / 下一首）
    s_btn_prev = create_transport_btn(screen, kBtnStartX, kBtnY, kBtnSize, btn_prev_cb);
    draw_triangle(screen, kBtnStartX + kBtnSize / 2 + 4, kBtnY + kBtnSize / 2, 10, -1);
    draw_triangle(screen, kBtnStartX + kBtnSize / 2 - 4, kBtnY + kBtnSize / 2, 10, -1);

    s_btn_play = create_transport_btn(screen, kBtnPlayX, kBtnY, kBtnSize, btn_play_cb);
    s_play_icon_is_pause = false;
    s_play_icon_lines[0] = draw_triangle(screen, kBtnPlayX + kBtnSize / 2, kBtnY + kBtnSize / 2, 14, 1);

    s_btn_next = create_transport_btn(screen, kBtnNextX, kBtnY, kBtnSize, btn_next_cb);
    draw_triangle(screen, kBtnNextX + kBtnSize / 2 - 4, kBtnY + kBtnSize / 2, 10, 1);
    draw_triangle(screen, kBtnNextX + kBtnSize / 2 + 4, kBtnY + kBtnSize / 2, 10, 1);

    // 空闲占位
    s_idle_label = lv_label_create(screen);
    lv_obj_set_pos(s_idle_label, 0, kBtnY);
    lv_obj_set_size(s_idle_label, kDisplayWidth, kBtnSize);
    lv_obj_set_style_text_font(s_idle_label, &zh_font_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_idle_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_idle_label, lv_color_hex(0x999999), LV_PART_MAIN);
    lv_label_set_text(s_idle_label, "等待投放...");

    // 频谱条 + peak hold 指示线（按钮下方，初始高度为 1）
    for (int i = 0; i < kSpecBarCount; i++) {
        int x = kSpecStartX + i * (kSpecBarW + kSpecBarGap);
        lv_obj_t *bar = lv_obj_create(screen);
        lv_obj_set_pos(bar, x, kSpecY + kSpecBarMaxH - 1);
        lv_obj_set_size(bar, kSpecBarW, 1);
        lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        s_spec_bars[i] = bar;
        // Peak hold：2px 高的细线，初始隐藏
        lv_obj_t *peak = lv_obj_create(screen);
        lv_obj_set_pos(peak, x, kSpecY + kSpecBarMaxH - 1);
        lv_obj_set_size(peak, kSpecBarW, 2);
        lv_obj_set_style_bg_color(peak, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(peak, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(peak, 0, LV_PART_MAIN);
        lv_obj_clear_flag(peak, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(peak, LV_OBJ_FLAG_HIDDEN);
        s_spec_peaks[i] = peak;
    }

    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(kTag, "player page built (vinyl+info+controls layout)");
}

bool update_miplay_player_page(const struct tm &local)
{
    (void)local;

    lyric_bridge_poll();

    const MiPlayMediaSnapshot snap = miplay_media_snapshot_load();
    const MiPlayLyricSnapshot lyric_snap = miplay_lyric_snapshot_load();

    bool changed = false;
    const bool has_media = snap.state != kMiPlayMediaIdle;

    // 空闲/有媒体的切换
    if (s_idle_label) {
        bool show_idle = !has_media;
        changed |= set_obj_visible(s_idle_label, show_idle);
        changed |= set_obj_visible(s_vinyl_card, has_media);
        changed |= set_obj_visible(s_title_label, has_media);
        changed |= set_obj_visible(s_artist_label, has_media);
        changed |= set_obj_visible(s_progress_bar, has_media);
        changed |= set_obj_visible(s_cur_time_label, has_media);
        changed |= set_obj_visible(s_total_time_label, has_media);
        changed |= set_obj_visible(s_lyric_prev_label, has_media);
        changed |= set_obj_visible(s_lyric_label, has_media);
        changed |= set_obj_visible(s_lyric_next_label, has_media);
        changed |= set_obj_visible(s_btn_prev, has_media);
        changed |= set_obj_visible(s_btn_play, has_media);
        changed |= set_obj_visible(s_btn_next, has_media);
        for (int i = 0; i < kSpecBarCount; i++) {
            changed |= set_obj_visible(s_spec_bars[i], has_media);
            changed |= set_obj_visible(s_spec_peaks[i], has_media);
        }
    }

    if (!has_media) {
        return changed;
    }

    // 歌曲标题 + 歌手
    if (snap.meta_version != s_last_meta_version) {
        s_last_meta_version = snap.meta_version;
        if (s_title_label) {
            lv_label_set_text(s_title_label, snap.title);
        }
        if (s_artist_label) {
            lv_label_set_text(s_artist_label, snap.artist);
        }
        changed = true;
    }

    // 进度条
    if (snap.duration_ms > 0) {
        int progress = static_cast<int>((snap.position_ms * kProgressMax) / snap.duration_ms);
        if (progress < 0) progress = 0;
        if (progress > kProgressMax) progress = kProgressMax;
        if (progress != s_last_progress) {
            s_last_progress = progress;
            if (s_progress_bar) {
                lv_bar_set_value(s_progress_bar, progress, LV_ANIM_OFF);
            }
            changed = true;
        }
    }

    // 当前时间（进度条左侧，跟随位置更新）
    if (snap.position_ms != s_last_position_ms) {
        s_last_position_ms = snap.position_ms;
        if (s_cur_time_label) {
            char buf[16];
            format_time(buf, sizeof(buf), snap.position_ms);
            lv_label_set_text(s_cur_time_label, buf);
        }
        changed = true;
    }

    // 总时长（进度条右侧，仅元数据变化时更新）
    if (snap.duration_ms != s_last_duration_ms) {
        s_last_duration_ms = snap.duration_ms;
        if (s_total_time_label) {
            char buf[16];
            format_time(buf, sizeof(buf), snap.duration_ms);
            lv_label_set_text(s_total_time_label, buf);
        }
        changed = true;
    }

    // 播放/暂停图标切换
    if (snap.paused != s_last_paused || snap.playing != s_last_playing) {
        s_last_paused = snap.paused;
        s_last_playing = snap.playing;
        // 播放中→显示暂停图标(⏸)，暂停中→显示播放图标(▶)
        update_play_icon(!snap.paused);
        changed = true;
    }

    // 频谱条 + peak hold 更新：仅在有音频时刷新。
    if (has_media) {
        SpectrumData spec = spectrum_read();
        bool spec_changed = false;
        for (int i = 0; i < kSpecBarCount; i++) {
            int h = (int)(spec.bands[i] * kSpecBarMaxH);
            if (h < 1) h = 1;
            int x = kSpecStartX + i * (kSpecBarW + kSpecBarGap);
            if (s_spec_bars[i]) {
                int old_h = (int)lv_obj_get_height(s_spec_bars[i]);
                int dy = h - old_h;
                if (dy < 0) dy = -dy;
                if (dy >= 2) {
                    lv_obj_set_size(s_spec_bars[i], kSpecBarW, h);
                    lv_obj_set_pos(s_spec_bars[i],
                                   x, kSpecY + kSpecBarMaxH - h);
                    spec_changed = true;
                }
            }
            // Peak hold 指示线
            if (s_spec_peaks[i]) {
                int ph = (int)(spectrum_peak_hold(i) * kSpecBarMaxH);
                if (ph < 2) ph = 2;
                int peak_y = kSpecY + kSpecBarMaxH - ph;
                int old_peak_y = (int)lv_obj_get_y(s_spec_peaks[i]);
                int dpy = peak_y - old_peak_y;
                if (dpy < 0) dpy = -dpy;
                if (dpy >= 2) {
                    lv_obj_set_pos(s_spec_peaks[i], x, peak_y);
                    spec_changed = true;
                }
            }
        }
        if (spec_changed) changed = true;
    }

    // 3行歌词（上一句 / 当前 / 下一句）
    if (lyric_snap.version != s_last_lyric_version ||
        lyric_snap.current_index != s_last_lyric_index) {
        s_last_lyric_version = lyric_snap.version;
        s_last_lyric_index = lyric_snap.current_index;
        if (s_lyric_prev_label) {
            const char *prev = miplay_lyric_get_line(lyric_snap.current_index - 1);
            lv_label_set_text(s_lyric_prev_label, prev ? prev : "");
        }
        if (s_lyric_label) {
            const char *cur = miplay_lyric_get_line(lyric_snap.current_index);
            lv_label_set_text(s_lyric_label, cur ? cur : "");
        }
        if (s_lyric_next_label) {
            const char *next = miplay_lyric_get_line(lyric_snap.current_index + 1);
            lv_label_set_text(s_lyric_next_label, next ? next : "");
        }
        changed = true;
    }

    return changed;
}

void clear_miplay_player_object_refs()
{
    s_screen = nullptr;
    s_vinyl_card = nullptr;
    s_title_label = nullptr;
    s_artist_label = nullptr;
    s_lyric_prev_label = nullptr;
    s_lyric_label = nullptr;
    s_lyric_next_label = nullptr;
    s_progress_bar = nullptr;
    s_cur_time_label = nullptr;
    s_total_time_label = nullptr;
    s_btn_prev = nullptr;
    s_btn_play = nullptr;
    s_btn_next = nullptr;
    for (int i = 0; i < 4; i++) s_play_icon_lines[i] = nullptr;
    s_idle_label = nullptr;
    for (int i = 0; i < kSpecBarCount; i++) s_spec_bars[i] = nullptr;
    for (int i = 0; i < kSpecBarCount; i++) s_spec_peaks[i] = nullptr;
    s_last_lyric_index = -1;
    s_last_playing = false;
    s_last_paused = false;
    s_last_position_ms = -1;
    s_last_duration_ms = -1;
    s_last_meta_version = 0;
    s_last_lyric_version = 0;
    s_last_progress = -1;
}
