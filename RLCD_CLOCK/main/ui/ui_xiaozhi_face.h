// 声明小智页面单色表情的固定尺寸、动画节拍和绘制入口。
#pragma once

#include "lvgl.h"
#include "xiaozhi_ai.h"

inline constexpr int kXiaozhiFaceCanvasWidth = 76;
inline constexpr int kXiaozhiFaceCanvasHeight = 76;
inline constexpr uint32_t kXiaozhiFaceFrameIntervalMs = 250;

bool update_xiaozhi_face(lv_obj_t *canvas, const XiaozhiAiSnapshot &snapshot);
void invalidate_xiaozhi_face_cache();
