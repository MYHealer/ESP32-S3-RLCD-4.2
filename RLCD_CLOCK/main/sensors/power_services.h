// 声明网络与音频电源管理锁的初始化、状态和嵌套所有权接口。
#pragma once

struct PowerLockDepthSnapshot {
    int network = 0;
    int audio = 0;
    int audio_wake = 0;
    int audio_cpu = 0;
};

[[nodiscard]] bool acquire_network_awake_lock();
void release_network_awake_lock();
bool network_awake_lock_active();
bool get_power_lock_depth_snapshot(PowerLockDepthSnapshot *out);
[[nodiscard]] bool acquire_audio_awake_lock();
void release_audio_awake_lock();

// 常驻发现服务（MiPlay/airkan）的电源策略开关。
// 启用后：
//   1. network awake lock 由服务生命周期持有（射频不关停）；
//   2. wifi_always_on_required() 返回 true——所有 esp_wifi_set_ps()
//      调用点必须据此拦截 WIFI_PS_MAX_MODEM，因为 modem sleep 会让 AP
//      缓存组播帧，手机 mDNS 查询在 DTIM 间隙整条丢失，设备无法被发现。
void set_discovery_services_power_policy(bool enabled);
bool wifi_always_on_required();
