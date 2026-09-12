// 构建并刷新小智 AI 工作页，复用反显时钟并以单色表情呈现官方情绪。
#include "ui_xiaozhi.h"
#include "ui_views.h"

#include "app_metadata.h"
#include "ui_battery.h"
#include "ui_fonts.h"
#include "ui_work_page_layout.h"
#include "ui_xiaozhi_face.h"
#include "ui_xiaozhi_subtitle.h"
#include "pomodoro_services.h"
#include "xiaozhi_ai.h"

#include <esp_attr.h>
#include <esp_log.h>

namespace {
constexpr int kClockCardCount = 3;
constexpr int kInteractionPanelX = 18;
constexpr int kInteractionPanelY = 188;
constexpr int kInteractionPanelW = 364;
constexpr int kInteractionPanelH = 102;
constexpr int kInteractionPanelRadius = 18;
constexpr int kFaceX = 30;
constexpr int kFaceY = 201;
constexpr int kStateX = 118;
constexpr int kStateY = 196;
constexpr int kStateW = 248;
constexpr int kStateH = 28;
constexpr int kPreparingDotsX = 218;
constexpr int kPreparingDotsY = 201;
constexpr int kPreparingDotsW = 48;
constexpr int kPreparingDotsH = 16;
constexpr int kPreparingDotRadius = 4;
constexpr int kPreparingDotCenterY = kPreparingDotsH / 2;
constexpr int kPreparingDotCenterX[] = {6, 22, 38};
constexpr int kDetailX = 118;
constexpr int kDetailY = 224;
constexpr int kDetailW = 248;
constexpr int kDetailH = 58;
constexpr uint32_t kPreparingDotsIntervalMs = 400;
constexpr const char *kBindingPrefix = "绑定 ID: ";
constexpr const char *kPreparingStatus = "小智准备中";
constexpr int kPomodoroTitleY = 8;
constexpr int kPomodoroStateY = 44;
constexpr int kPomodoroModeY = 76;
constexpr int kPomodoroTextW = 112;
constexpr int kPomodoroTextH = 24;
constexpr int kPomodoroTitleH = 30;
constexpr const char *kPomodoroTitle = "番茄钟";
constexpr const char *kPomodoroRunningText = "专注中";
constexpr const char *kPomodoroCompletedText = "已完成";
constexpr const char *kPomodoroMinuteSecondMode = "分 / 秒";
constexpr const char *kWaveCanvasCreateFailedLog = "Xiaozhi wave canvas create failed";
constexpr const char *kPreparingDotsCanvasCreateFailedLog =
    "Xiaozhi preparing dots canvas create failed";

lv_obj_t *s_clock_cards[kClockCardCount] = {};
lv_color_t *s_clock_card_buffers[kClockCardCount] = {};
lv_obj_t *s_xiaozhi_state_label = nullptr;
lv_obj_t *s_xiaozhi_detail_label = nullptr;
lv_obj_t *s_xiaozhi_wave_canvas = nullptr;
lv_color_t *s_face_canvas_buffer = nullptr;
int s_last_clock_values[kClockCardCount] = {-1, -1, -1};
XiaozhiAiState s_animation_state = kXiaozhiAiInactive;
lv_obj_t *s_pomodoro_title_label = nullptr;
lv_obj_t *s_pomodoro_title_bold_label = nullptr;
lv_obj_t *s_pomodoro_state_label = nullptr;
lv_obj_t *s_pomodoro_mode_label = nullptr;
lv_obj_t *s_preparing_dots_canvas = nullptr;
lv_color_t *s_preparing_dots_buffer = nullptr;
PomodoroState s_last_pomodoro_state = kPomodoroIdle;
int s_last_preparing_dot_count = -1;
EXT_RAM_BSS_ATTR XiaozhiAiSnapshot s_xiaozhi_snapshot_cache = {};
uint32_t s_xiaozhi_snapshot_version = 0;

struct PomodoroDisplayValues {
    int second_card;
    int third_card;
};

constexpr PomodoroDisplayValues pomodoro_display_values(uint32_t remaining_ms)
{
    uint32_t remaining_seconds = pomodoro_display_seconds(remaining_ms);
    return {static_cast<int>(remaining_seconds / 60U),
            static_cast<int>(remaining_seconds % 60U)};
}

constexpr PomodoroDisplayValues kPomodoroAtSixtySeconds = pomodoro_display_values(60000U);
constexpr PomodoroDisplayValues kPomodoroAtFiftyNineSeconds = pomodoro_display_values(59000U);
static_assert(kPomodoroAtSixtySeconds.second_card == 1 &&
                  kPomodoroAtSixtySeconds.third_card == 0,
              "60 seconds must remain one minute and zero seconds");
static_assert(kPomodoroAtFiftyNineSeconds.second_card == 0 &&
                  kPomodoroAtFiftyNineSeconds.third_card == 59,
              "59 seconds must show zero minutes and 59 seconds");

lv_obj_t *create_xiaozhi_canvas(lv_obj_t *parent,
                                lv_color_t *buffer,
                                int x,
                                int y,
                                int width,
                                int height,
                                const char *failure_log)
{
    if (!parent || !buffer) {
        return nullptr;
    }
    lv_obj_t *canvas = lv_canvas_create(parent);
    if (!canvas) {
        ESP_LOGW(TAG, "%s", failure_log);
        return nullptr;
    }
    configure_canvas_base(canvas, buffer, x, y, width, height);
    return canvas;
}

lv_obj_t *make_pomodoro_card_label(lv_obj_t *parent,
                                    int y,
                                    int height,
                                    const char *text,
                                    const lv_font_t *font)
{
    lv_obj_t *label = make_label_with_font(parent,
                                           0,
                                           y,
                                           kPomodoroTextW,
                                           height,
                                           text,
                                           font);
    if (label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
    return label;
}

void set_pomodoro_labels_visible(bool visible)
{
    set_obj_visible(s_pomodoro_title_label, visible);
    set_obj_visible(s_pomodoro_title_bold_label, visible);
    set_obj_visible(s_pomodoro_state_label, visible);
    set_obj_visible(s_pomodoro_mode_label, visible);
}

void draw_preparing_dots(int count)
{
    if (!s_preparing_dots_canvas || count == s_last_preparing_dot_count) {
        return;
    }
    lv_canvas_fill_bg(s_preparing_dots_canvas, lv_color_black(), LV_OPA_COVER);
    for (int i = 0; i < count && i < 3; ++i) {
        canvas_draw_filled_circle(s_preparing_dots_canvas,
                                  kPreparingDotsW,
                                  kPreparingDotsH,
                                  kPreparingDotCenterX[i],
                                  kPreparingDotCenterY,
                                  kPreparingDotRadius,
                                  lv_color_white());
    }
    s_last_preparing_dot_count = count;
    lv_obj_invalidate(s_preparing_dots_canvas);
}

bool update_xiaozhi_clock_or_pomodoro(const struct tm &local,
                                      const PomodoroSnapshot &pomodoro)
{
    if (pomodoro.state == kPomodoroIdle) {
        if (s_last_pomodoro_state != kPomodoroIdle) {
            set_pomodoro_labels_visible(false);
            for (int &value : s_last_clock_values) {
                value = -1;
            }
        }
        s_last_pomodoro_state = kPomodoroIdle;
        return update_inverted_clock_cards(local, s_clock_cards, s_last_clock_values);
    }

    bool state_changed = pomodoro.state != s_last_pomodoro_state;
    PomodoroDisplayValues display = pomodoro_display_values(pomodoro.remaining_ms);
    bool changed = false;
    if (state_changed) {
        clear_inverted_clock_card(s_clock_cards[0]);
        set_pomodoro_labels_visible(true);
        s_last_clock_values[0] = -1;
        s_last_clock_values[1] = -1;
        s_last_clock_values[2] = -1;
        changed = true;
    }
    if (state_changed) {
        changed |= set_label_text_if_changed(s_pomodoro_title_label, kPomodoroTitle);
        changed |= set_label_text_if_changed(s_pomodoro_title_bold_label, kPomodoroTitle);
        changed |= set_label_text_if_changed(
            s_pomodoro_state_label,
            pomodoro.state == kPomodoroCompleted ? kPomodoroCompletedText : kPomodoroRunningText);
        changed |= set_label_text_if_changed(
            s_pomodoro_mode_label,
            pomodoro.state == kPomodoroCompleted
                ? ""
                : kPomodoroMinuteSecondMode);
    }

    int second_card_value = 0;
    int third_card_value = 0;
    if (pomodoro.state == kPomodoroRunning) {
        second_card_value = display.second_card;
        third_card_value = display.third_card;
    }
    changed |= update_inverted_clock_card_value(s_clock_cards[1],
                                                second_card_value,
                                                &s_last_clock_values[1]);
    changed |= update_inverted_clock_card_value(s_clock_cards[2],
                                                third_card_value,
                                                &s_last_clock_values[2]);
    s_last_pomodoro_state = pomodoro.state;
    return changed;
}

void style_panel_label(lv_obj_t *label, lv_text_align_t align)
{
    if (!label) {
        return;
    }
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
}
} // namespace

void build_xiaozhi_page()
{
    if (work_page_root(kWorkPageXiaozhiAI)) {
        return;
    }
    lv_obj_t *root = create_page_root();
    if (!root) {
        return;
    }
    set_work_page_root(kWorkPageXiaozhiAI, root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    build_work_page_status_bar(root,
                               kWorkPageXiaozhiAI,
                               true,
                               true);
    set_obj_visible(get_work_page_status_labels(kWorkPageXiaozhiAI).time, false);
    make_black_bar(root,
                   ui_work_page_layout::kTopSeparatorX,
                   ui_work_page_layout::kTopSeparatorY,
                   ui_work_page_layout::kTopSeparatorWidth,
                   ui_work_page_layout::kTopSeparatorHeight);
    build_work_page_day_progress(root, kWorkPageXiaozhiAI);
    build_inverted_clock_cards(root, s_clock_cards, s_clock_card_buffers);
    s_pomodoro_title_label = make_pomodoro_card_label(s_clock_cards[0],
                                                       kPomodoroTitleY,
                                                       kPomodoroTitleH,
                                                       kPomodoroTitle,
                                                       &zh_pomodoro_title_24);
    s_pomodoro_title_bold_label = make_pomodoro_card_label(s_clock_cards[0],
                                                            kPomodoroTitleY,
                                                            kPomodoroTitleH,
                                                            kPomodoroTitle,
                                                            &zh_pomodoro_title_24);
    if (s_pomodoro_title_bold_label) {
        lv_obj_set_x(s_pomodoro_title_bold_label, 1);
    }
    s_pomodoro_state_label = make_pomodoro_card_label(s_clock_cards[0],
                                                       kPomodoroStateY,
                                                       kPomodoroTextH,
                                                       kPomodoroRunningText,
                                                       &zh_font_16);
    s_pomodoro_mode_label = make_pomodoro_card_label(s_clock_cards[0],
                                                      kPomodoroModeY,
                                                      kPomodoroTextH,
                                                      kPomodoroMinuteSecondMode,
                                                      &zh_font_16);
    set_pomodoro_labels_visible(false);
    s_last_clock_values[0] = -1;
    s_last_clock_values[1] = -1;
    s_last_clock_values[2] = -1;

    lv_obj_t *interaction_panel = make_black_bar(root,
                                                 kInteractionPanelX,
                                                 kInteractionPanelY,
                                                 kInteractionPanelW,
                                                 kInteractionPanelH);
    if (interaction_panel) {
        lv_obj_set_style_radius(interaction_panel, kInteractionPanelRadius, LV_PART_MAIN);
        lv_obj_set_style_clip_corner(interaction_panel, true, LV_PART_MAIN);
    }

    if (!s_face_canvas_buffer) {
        s_face_canvas_buffer = alloc_canvas_buffer(kXiaozhiFaceCanvasWidth,
                                                   kXiaozhiFaceCanvasHeight);
    }
    s_xiaozhi_wave_canvas = create_xiaozhi_canvas(root,
                                                   s_face_canvas_buffer,
                                                   kFaceX,
                                                   kFaceY,
                                                   kXiaozhiFaceCanvasWidth,
                                                   kXiaozhiFaceCanvasHeight,
                                                   kWaveCanvasCreateFailedLog);

    s_xiaozhi_state_label = make_label_with_font(root,
                                                  kStateX,
                                                  kStateY,
                                                  kStateW,
                                                  kStateH,
                                                  "",
                                                  &zh_font_16);
    s_xiaozhi_detail_label = make_label_with_font(root,
                                                   kDetailX,
                                                   kDetailY,
                                                   kDetailW,
                                                   kDetailH,
                                                   "",
                                                   &zh_font_16);
    style_panel_label(s_xiaozhi_state_label, LV_TEXT_ALIGN_LEFT);
    style_panel_label(s_xiaozhi_detail_label, LV_TEXT_ALIGN_LEFT);
    if (!s_preparing_dots_buffer) {
        s_preparing_dots_buffer = alloc_canvas_buffer(kPreparingDotsW, kPreparingDotsH);
    }
    s_preparing_dots_canvas = create_xiaozhi_canvas(root,
                                                     s_preparing_dots_buffer,
                                                     kPreparingDotsX,
                                                     kPreparingDotsY,
                                                     kPreparingDotsW,
                                                     kPreparingDotsH,
                                                     kPreparingDotsCanvasCreateFailedLog);
    if (s_preparing_dots_canvas) {
        set_obj_visible(s_preparing_dots_canvas, false);
    }
    s_last_preparing_dot_count = -1;
    if (s_xiaozhi_detail_label) {
        lv_label_set_long_mode(s_xiaozhi_detail_label, LV_LABEL_LONG_WRAP);
    }
    build_work_page_battery_icon(root, kWorkPageXiaozhiAI);
}

bool update_xiaozhi_page(const struct tm &local)
{
    if (!work_page_root(kWorkPageXiaozhiAI)) {
        build_xiaozhi_page();
    }
    (void)xiaozhi_ai_get_snapshot_if_changed(&s_xiaozhi_snapshot_version,
                                              &s_xiaozhi_snapshot_cache);
    const XiaozhiAiSnapshot &snapshot = s_xiaozhi_snapshot_cache;
    PomodoroSnapshot pomodoro = {};
    pomodoro_get_snapshot(&pomodoro);
    s_animation_state = snapshot.state;

    char detail[192] = {};
    if (snapshot.binding_code[0] != '\0') {
        snprintf(detail, sizeof(detail), "%s%s", kBindingPrefix, snapshot.binding_code);
    } else {
        strlcpy(detail, snapshot.detail, sizeof(detail));
    }
    const char *display_detail = xiaozhi_latest_visible_subtitle(
        xiaozhi_progressive_subtitle(snapshot.state == kXiaozhiAiSpeaking, detail),
        &zh_font_16,
        kDetailW,
        kDetailH);
    bool changed = false;
    const char *display_status = snapshot.status;
    if (snapshot.state == kXiaozhiAiInactive) {
        display_status = kPreparingStatus;
        int dots = 1 + static_cast<int>((lv_tick_get() / kPreparingDotsIntervalMs) % 3U);
        if (dots != s_last_preparing_dot_count) {
            draw_preparing_dots(dots);
            changed = true;
        }
        changed |= set_obj_visible(s_preparing_dots_canvas, true);
    } else {
        changed |= set_obj_visible(s_preparing_dots_canvas, false);
    }
    changed |= set_label_text_if_changed(s_xiaozhi_state_label, display_status);
    changed |= set_label_text_if_changed(s_xiaozhi_detail_label, display_detail);
    const bool status_time_visible = pomodoro.state != kPomodoroIdle;
    const WorkPageStatusLabels status = get_work_page_status_labels(kWorkPageXiaozhiAI);
    changed |= set_obj_visible(status.time, status_time_visible);
    if (status_time_visible) {
        changed |= update_work_page_status_time(kWorkPageXiaozhiAI, local);
    }
    changed |= update_xiaozhi_clock_or_pomodoro(local, pomodoro);
    changed |= update_xiaozhi_face(s_xiaozhi_wave_canvas, snapshot);
    return changed;
}

uint32_t xiaozhi_subtitle_animation_delay_ms()
{
    uint32_t delay_ms = xiaozhi_subtitle_next_delay_ms();
    bool expression_animated = s_animation_state == kXiaozhiAiActivating ||
                               s_animation_state == kXiaozhiAiBinding ||
                               s_animation_state == kXiaozhiAiListening ||
                               s_animation_state == kXiaozhiAiSpeaking;
    if (expression_animated &&
        (delay_ms == 0 || delay_ms > kXiaozhiFaceFrameIntervalMs)) {
        delay_ms = kXiaozhiFaceFrameIntervalMs;
    }
    if (s_animation_state == kXiaozhiAiInactive) {
        uint32_t preparing_delay = kPreparingDotsIntervalMs -
                                   (lv_tick_get() % kPreparingDotsIntervalMs);
        if (delay_ms == 0 || preparing_delay < delay_ms) {
            delay_ms = preparing_delay;
        }
    }
    return delay_ms;
}

void clear_xiaozhi_page_object_refs()
{
    for (lv_obj_t *&card : s_clock_cards) {
        card = nullptr;
    }
    s_xiaozhi_state_label = nullptr;
    s_xiaozhi_detail_label = nullptr;
    s_xiaozhi_wave_canvas = nullptr;
    s_pomodoro_title_label = nullptr;
    s_pomodoro_title_bold_label = nullptr;
    s_pomodoro_state_label = nullptr;
    s_pomodoro_mode_label = nullptr;
    s_preparing_dots_canvas = nullptr;
    for (int &value : s_last_clock_values) {
        value = -1;
    }
    s_animation_state = kXiaozhiAiInactive;
    s_last_pomodoro_state = kPomodoroIdle;
    s_last_preparing_dot_count = -1;
    s_xiaozhi_snapshot_cache = {};
    s_xiaozhi_snapshot_version = 0;
    invalidate_xiaozhi_face_cache();
}
