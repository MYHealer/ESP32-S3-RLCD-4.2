// 构建 SDL 启动页、驱动启动动画并保存固定帧截图。
#pragma once

struct SdlPreviewBackend;

void build_boot_preview_screen(const char *app_version);
bool save_boot_preview_if_requested(SdlPreviewBackend *backend,
                                    const char *screenshot_path,
                                    const char *preview_mode);
void run_boot_preview_animation();
