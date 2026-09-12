// 将小智状态和情绪映射为单色表情，并维护页面表情动画缓存。
#include "ui_xiaozhi_face.h"

#include "ui_canvas_primitives.h"

#include <string.h>

namespace {
constexpr int kFaceCenterX = kXiaozhiFaceCanvasWidth / 2;
constexpr int kFaceCenterY = kXiaozhiFaceCanvasHeight / 2;

enum FaceEmotion {
    kFaceNeutral,
    kFaceHappy,
    kFaceSad,
    kFaceAngry,
    kFaceSurprised,
    kFaceThinking,
    kFaceSleepy,
    kFaceCool,
};

lv_obj_t *s_last_face_canvas = nullptr;
XiaozhiAiState s_last_face_state = kXiaozhiAiInactive;
int s_last_face_frame = -1;
char s_last_face_emotion[24] = {};

bool emotion_is(const char *emotion, const char *value)
{
    return emotion && strcmp(emotion, value) == 0;
}

FaceEmotion face_emotion_for_snapshot(const XiaozhiAiSnapshot &snapshot)
{
    const char *emotion = snapshot.emotion;
    if (emotion_is(emotion, "sad") || emotion_is(emotion, "crying")) return kFaceSad;
    if (emotion_is(emotion, "angry")) return kFaceAngry;
    if (emotion_is(emotion, "surprised") || emotion_is(emotion, "shocked")) return kFaceSurprised;
    if (emotion_is(emotion, "thinking") || emotion_is(emotion, "confused") ||
        emotion_is(emotion, "embarrassed")) return kFaceThinking;
    if (emotion_is(emotion, "sleepy")) return kFaceSleepy;
    if (emotion_is(emotion, "cool")) return kFaceCool;
    if (emotion_is(emotion, "happy") || emotion_is(emotion, "laughing") ||
        emotion_is(emotion, "funny") || emotion_is(emotion, "loving") ||
        emotion_is(emotion, "winking") || emotion_is(emotion, "relaxed") ||
        emotion_is(emotion, "delicious") || emotion_is(emotion, "kissy") ||
        emotion_is(emotion, "confident") || emotion_is(emotion, "silly")) return kFaceHappy;
    if (snapshot.state == kXiaozhiAiError) return kFaceSad;
    return kFaceNeutral;
}

void draw_face_outline(lv_obj_t *canvas)
{
    canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight,
                              kFaceCenterX, kFaceCenterY, 32, lv_color_white());
    canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight,
                              kFaceCenterX, kFaceCenterY, 29, lv_color_black());
}

void draw_open_eye(lv_obj_t *canvas, int x, int y, int radius = 3)
{
    canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight,
                              x, y, radius, lv_color_white());
}

void draw_happy_mouth(lv_obj_t *canvas, bool inverted)
{
    const int direction = inverted ? -1 : 1;
    int y = inverted ? 53 : 45;
    canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 25, y, 30, y + 5 * direction, lv_color_white());
    canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 30, y + 5 * direction, 38, y + 7 * direction, lv_color_white());
    canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 38, y + 7 * direction, 46, y + 5 * direction, lv_color_white());
    canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 46, y + 5 * direction, 51, y, lv_color_white());
}

void draw_expression(lv_obj_t *canvas, const XiaozhiAiSnapshot &snapshot, int frame)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    draw_face_outline(canvas);
    FaceEmotion emotion = face_emotion_for_snapshot(snapshot);
    int eye_y = 30;

    if (emotion == kFaceHappy) {
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 22, 31, 27, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 27, 27, 32, 31, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 44, 31, 49, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 49, 27, 54, 31, lv_color_white());
    } else if (emotion == kFaceAngry) {
        draw_open_eye(canvas, 27, eye_y);
        draw_open_eye(canvas, 49, eye_y);
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 20, 23, 32, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 44, 27, 56, 23, lv_color_white());
    } else if (emotion == kFaceSleepy) {
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 22, eye_y, 32, eye_y, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 44, eye_y, 54, eye_y, lv_color_white());
    } else if (emotion == kFaceCool) {
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 19, 27, 34, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 42, 27, 57, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 34, 27, 42, 27, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 20, 28, 23, 35, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 56, 28, 53, 35, lv_color_white());
    } else if (emotion == kFaceThinking) {
        draw_open_eye(canvas, 27, eye_y);
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 44, eye_y, 54, eye_y - 3, lv_color_white());
    } else {
        draw_open_eye(canvas, 27, eye_y,
                      snapshot.state == kXiaozhiAiReady && frame == 3 ? 1 : 3);
        draw_open_eye(canvas, 49, eye_y,
                      snapshot.state == kXiaozhiAiReady && frame == 3 ? 1 : 3);
    }

    if (snapshot.state == kXiaozhiAiSpeaking) {
        int mouth_radius = 5 + (frame % 2) * 3;
        canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 38, 51, mouth_radius, lv_color_white());
        if (mouth_radius > 5) {
            canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 38, 51, mouth_radius - 3, lv_color_black());
        }
    } else if (emotion == kFaceHappy) {
        draw_happy_mouth(canvas, false);
    } else if (emotion == kFaceSad || emotion == kFaceAngry) {
        draw_happy_mouth(canvas, true);
    } else if (emotion == kFaceSurprised) {
        canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 38, 50, 7, lv_color_white());
        canvas_draw_filled_circle(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 38, 50, 4, lv_color_black());
    } else if (emotion == kFaceThinking) {
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 33, 51, 47, 48, lv_color_white());
    } else {
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 29, 50, 47, 50, lv_color_white());
    }

    if (snapshot.state == kXiaozhiAiListening) {
        int inset = frame % 2;
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 3 + inset, 29, 3 + inset, 47, lv_color_white());
        canvas_draw_line(canvas, kXiaozhiFaceCanvasWidth, kXiaozhiFaceCanvasHeight, 72 - inset, 29, 72 - inset, 47, lv_color_white());
    } else if (snapshot.state == kXiaozhiAiActivating || snapshot.state == kXiaozhiAiBinding) {
        int dot_x[4] = {38, 58, 38, 18};
        int dot_y[4] = {10, 38, 66, 38};
        draw_open_eye(canvas, dot_x[frame], dot_y[frame], 2);
    }
}
} // namespace

bool update_xiaozhi_face(lv_obj_t *canvas, const XiaozhiAiSnapshot &snapshot)
{
    if (!canvas) {
        return false;
    }
    bool animated = snapshot.state == kXiaozhiAiActivating ||
                    snapshot.state == kXiaozhiAiBinding ||
                    snapshot.state == kXiaozhiAiListening ||
                    snapshot.state == kXiaozhiAiSpeaking;
    int frame = animated
                    ? static_cast<int>((lv_tick_get() / kXiaozhiFaceFrameIntervalMs) % 4)
                    : 0;
    if (s_last_face_canvas == canvas &&
        s_last_face_state == snapshot.state &&
        s_last_face_frame == frame &&
        strcmp(s_last_face_emotion, snapshot.emotion) == 0) {
        return false;
    }

    draw_expression(canvas, snapshot, frame);
    lv_obj_invalidate(canvas);
    s_last_face_canvas = canvas;
    s_last_face_state = snapshot.state;
    s_last_face_frame = frame;
    strlcpy(s_last_face_emotion, snapshot.emotion, sizeof(s_last_face_emotion));
    return true;
}

void invalidate_xiaozhi_face_cache()
{
    s_last_face_canvas = nullptr;
    s_last_face_state = kXiaozhiAiInactive;
    s_last_face_frame = -1;
    s_last_face_emotion[0] = '\0';
}
