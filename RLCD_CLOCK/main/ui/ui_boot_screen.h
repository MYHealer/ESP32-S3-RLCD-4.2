// 声明启动屏、启动动画和联网进度显示的轻量生命周期接口。
#pragma once

void prepare_boot_animation();
void request_boot_animation_stop();
void boot_anim_task(void *);
void finish_boot_anim_to_last_frame();
void show_boot_screen();
void update_boot_screen(int percent, const char *status, const char *detail);
bool finish_boot_screen();
