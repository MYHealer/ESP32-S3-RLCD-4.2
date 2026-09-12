// MiPlay 遥控接收端：让设备被小米「设备互联」识别为 TV，并接收遥控按键。
//
// 本组件从 esp-miply-1.85touch 的 components/miplay 移植，只保留
// 「被手机发现 + 建立控制会话」链路，剥离了 RTSP 投屏、音频管线、
// DLNA、歌词封面等无关功能。
//
// 链路全景（四层，缺一不可）：
//   ① 局域网发现：mDNS 注册(_mi-connect/_lyra-mdns) + UDP 5355 DNS-SD 应答
//      + mDNS 主动宣告
//   ② TCP 8899 控制口：手机据此建立 MiPlay 会话并获知设备 IP
//   ③ HTTP 6095 认证：airkan requestAuth/completeAuth（第二阶段）
//   ④ TCP 6091 长连：接收遥控按键帧（第二阶段）
//
// 本次实现 ①②。③④ 的代码已在源工程验证通过，待第二阶段移入。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动 MiPlay 遥控接收端：初始化身份、注册 mDNS、启动发现与 8899 监听。
// 需要 Wi-Fi 已连接（依赖 STA netif 的 IP）。
// 返回 ESP_OK 表示服务已启动；失败时调用方不应为其持有射频唤醒锁。
// 重复调用是安全的（只生效一次，返回 ESP_OK）。
esp_err_t miplay_remote_start(void);

// 停止所有相关任务并关闭监听 socket。
void miplay_remote_stop(void);

// 是否有手机成功建立控制会话。
bool miplay_remote_is_connected(void);

#ifdef __cplusplus
}
#endif
