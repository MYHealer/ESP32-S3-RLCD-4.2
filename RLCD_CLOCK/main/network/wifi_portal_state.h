// 声明 Wi-Fi 门户会话、断线原因、AP 名称和本地 IP 的跨任务安全访问接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t kWifiStationIpTextLen = 16;
inline constexpr size_t kWifiSetupApSsidTextLen = 33;

enum class WifiPortalSaveResult : uint8_t {
    kNone = 0,
    kValidating,
    kInvalidInput,
    kWifiConnectionFailed,
    kWeatherApiFailed,
    kWeatherCityInvalid,
    kSuccess,
};

struct WifiPortalSaveSnapshot {
    WifiPortalSaveResult result = WifiPortalSaveResult::kNone;
    bool feedback_seen = false;
    uint32_t generation = 0;
};

inline constexpr bool wifi_portal_result_preserves_client_lease(
    WifiPortalSaveResult result)
{
    return result == WifiPortalSaveResult::kValidating ||
           result == WifiPortalSaveResult::kSuccess;
}

bool setup_portal_active_load();
int wifi_last_disconnect_reason();
bool wifi_setup_ap_ssid_snapshot(char *out, size_t out_len);
bool wifi_station_ip_snapshot(char *out, size_t out_len);
WifiPortalSaveSnapshot wifi_portal_save_snapshot_load();
WifiPortalSaveResult wifi_portal_save_result_load();
bool wifi_portal_save_feedback_seen_load();
