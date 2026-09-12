// 声明应用级 FreeRTOS 事件组的生命周期和空句柄安全访问接口。
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Keep application event identifiers beside their only runtime owner so
// producers and consumers do not need the aggregate application-state header.
inline constexpr EventBits_t kWifiConnectedBit = 1U << 0;
inline constexpr EventBits_t kTimeSyncedBit = 1U << 1;
inline constexpr EventBits_t kWeatherReadyBit = 1U << 2;
inline constexpr EventBits_t kProvisioningSyncBit = 1U << 3;
inline constexpr EventBits_t kManualNtpSyncBit = 1U << 4;
inline constexpr EventBits_t kManualWeatherSyncBit = 1U << 5;
inline constexpr EventBits_t kOtaCheckBit = 1U << 6;
inline constexpr EventBits_t kOtaInstallBit = 1U << 7;
inline constexpr EventBits_t kManualSayingSyncBit = 1U << 8;
inline constexpr EventBits_t kBootSyncDoneBit = 1U << 9;
inline constexpr EventBits_t kBootAnimDoneBit = 1U << 10;
inline constexpr EventBits_t kNetworkDiagBit = 1U << 11;
// This bit wakes the network task after runtime configuration changes. It is
// not a sync request and must never be cleared with the request-bit group.
inline constexpr EventBits_t kNetworkStateChangedBit = 1U << 12;
// UI tasks only request the transition. The network task starts the setup AP
// after any in-flight HTTPS window has released Wi-Fi and lwIP resources.
inline constexpr EventBits_t kSetupPortalStartBit = 1U << 13;
// The captive portal publishes this edge after the phone has received the
// provisioning result, when a newer save supersedes an older result wait, or
// when the portal closes while the network task waits.
inline constexpr EventBits_t kProvisioningFeedbackBit = 1U << 14;
// Visible-page requests are cancellable when the user leaves the page. Keep
// them separate from settings requests so navigation cannot cancel an
// explicit user action.
inline constexpr EventBits_t kVisibleWeatherSyncBit = 1U << 15;
inline constexpr EventBits_t kVisibleSayingSyncBit = 1U << 16;
// SNTP publishes this edge after applying a network time sample. Only the
// active NTP wait consumes it; it is not a network-sync request.
inline constexpr EventBits_t kNtpSyncCompletedBit = 1U << 17;
// Page exit and alarm suspension publish this level while Xiaozhi is inactive.
// It lets a pending Wi-Fi connection wait release high-power ownership without
// borrowing or consuming the shared network-runtime change edge.
inline constexpr EventBits_t kXiaozhiPageStateChangedBit = 1U << 18;

bool app_event_group_ready();

EventBits_t app_event_group_set_bits(EventBits_t bits);
EventBits_t app_event_group_clear_bits(EventBits_t bits);
EventBits_t app_event_group_get_bits();
EventBits_t app_event_group_wait_bits(EventBits_t bits,
                                      BaseType_t clear_on_exit,
                                      BaseType_t wait_for_all,
                                      TickType_t timeout);
