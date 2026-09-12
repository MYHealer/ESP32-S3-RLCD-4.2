// MiPlay 遥控接收端内部共享定义：常量、身份状态、DNS 编码原语。
//
// 常量的取值来自已验证的参考实现，改动前务必确认其作用：
//   校验相关（改了手机就发现不了设备）：
//     - idHash 派生算法（SHA256 → base64url 3 字符 → 标准 base64）
//     - appsData / appsData 硬编码值
//     - lyra AppData 的字节序列，其中 appdata[2]=0x03 表示 TV（0x15 是 PC）
//     - UDP 5355 TXT 的 "type":3
//   展示相关（可改，但保持字节长度不变以免破坏 AppData 结构）：
//     - kDeviceDisplayName
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "mdns.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── 设备显示名 ──
// 与参考实现的 "ESP-1.85c" 同为 8 字符，保证 lyra AppData 的长度字节与
// base64 输出长度不变，只替换内容。改名是为了避免同网内两台设备重名。
//
// ⚠ 严禁出现 '.'：此名字会直接进入 DNS-SD 实例名（"<名字>(<后缀>)"）。
// 点号是 DNS label 分隔符，手机解析 PTR 时会把它当成两个 label，
// 后面 SRV/TXT 的压缩指针偏移随之错乱，整包被判为畸形而丢弃——
// 表现就是"设备明明应答了 5355，手机列表里却没有它"。
#define MIPLAY_DEVICE_DISPLAY_NAME "RLCD42"

// ── mDNS 服务名（ESP-IDF 不自动加下划线，必须手动包含）──
#define MIPLAY_LYRA_SERVICE  "_lyra-mdns"
#define MIPLAY_LYRA_PROTO    "_udp"
#define MIPLAY_MICON_SERVICE "_mi-connect"
#define MIPLAY_MICON_PROTO   "_udp"

// ── 端口 ──
#define MIPLAY_CONTROL_PORT       8899   // TCP 控制口，手机据此建立会话
#define MIPLAY_COAP_PORT          56666  // _mi-connect 注册端口
#define MIPLAY_LAN_DISCOVERY_PORT 5355   // UDP legacy DNS-SD
#define MIPLAY_MDNS_PORT          5353   // 标准 mDNS

// ── MiLink 字段（dev=2 为 HyperOS 必需值）──
#define MIPLAY_DEV     "2"
#define MIPLAY_SEC     "2"
#define MIPLAY_VERSION "196608"  // 0x30000

// ── 组播地址 ──
#define MIPLAY_MDNS_GROUP "224.0.0.251"

// ── 身份状态（启动时算一次，之后只读）──
// 放在 PSRAM BSS：这些字符串合计约 1KB，内部 SRAM 紧张。
extern char s_device_id[16];      // MAC[2..5] 大写 hex，8 字符
extern char s_inst_name[40];      // "RLCD42(<10字符后缀>)"
extern char s_idhash[8];          // 4 字符，TXT idHash
extern char s_appsdata[16];       // 硬编码校验值
extern char s_mac_b64[12];        // MAC 的 base64
extern char s_lyra_appdata[96];   // lyra AppData base64
extern uint8_t s_mac[6];

// 全局运行开关：所有任务主循环依赖它退出。
extern volatile bool s_running;

// ── DNS 编码原语（发现层与宣告层共用）──
void dns_push_u16(uint8_t *p, size_t *o, uint16_t v);
void dns_push_u32(uint8_t *p, size_t *o, uint32_t v);
void dns_push_label(uint8_t *p, size_t *o, const char *label);
void dns_push_ptr(uint8_t *p, size_t *o, uint16_t offset);

// 取 STA 网卡的 IPv4（网络字节序）。失败返回 127.0.0.1。
uint32_t miplay_get_my_ipv4(void);

// 生成设备身份（device_id/idHash/appsData/lyra AppData/uuid）。
// 幂等：重复调用只算一次。
void miplay_init_device_identity(void);

// 注册 mDNS 服务（_mi-connect._udp + _lyra-mdns._udp）。
// 需要 STA 已取得 IP，否则 SRV 的 A 记录指向 127.0.0.1。
esp_err_t miplay_register_mdns_services(void);

// 启动发现层任务：UDP 5355 应答 + mDNS 主动宣告。
void miplay_start_discovery(void);

// 创建使用 PSRAM 栈的任务，失败时回退内部栈。
// 返回 pdPASS / pdFAIL。
int miplay_create_task(void (*task)(void *), const char *name,
                       uint32_t stack_bytes, void *arg,
                       unsigned priority, void **handle, int core);

// 删除当前任务，正确处理 PSRAM 静态栈的释放。
void miplay_delete_current_task(void);

#ifdef __cplusplus
}
#endif
