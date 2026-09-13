// airkan 遥控接收端接入层：把 airkan-sdk 接到本工程的 UI 与网络生命周期上。
//
// 分工：
//   airkan-sdk   — 6095 HTTP 认证 + 6091 TCP 遥控长连 + 帧解析（协议细节全在 SDK 里）
//   本文件       — 配置注入、任务创建（走 PSRAM 栈）、按键语义到 UI 的桥接
//
// 按键回调只在 airkan 自己的 task 线程里触发。切页走"改状态 + 通知 UI 任务"，
// 不直接碰 LVGL——airkan task 毫秒级返回，也不会因抢 LVGL 锁被 Task WDT 打死。
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动 airkan 遥控服务（6095 认证 + 6091 遥控）。
//
// 幂等：重复调用只生效一次。需在 STA 拿到 IP 后调用。
// 返回 ESP_OK 表示监听已绑定；失败只记日志，不影响调用方其他功能。
esp_err_t airkan_remote_start(void);

// 停止服务并断开当前遥控连接。幂等。
void airkan_remote_stop(void);

// 当前是否有手机处于可遥控状态（版本协商通过且连接建立）。
bool airkan_remote_is_connected(void);

// 请求将当前音量保存到 NVS。内部用单飞任务避免重复创建。
// 可从任意上下文调用（PSRAM 栈安全——保存任务用内部 RAM 栈）。
void request_volume_save(void);

#ifdef __cplusplus
}
#endif
