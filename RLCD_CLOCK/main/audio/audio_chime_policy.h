// 定义整点提醒和普通提示音的纯阻断策略。
#pragma once

enum AudioChimeDecision {
    kAudioChimePlay = 0,
    kAudioChimeBlockedBySystem,
    kAudioChimeBlockedByNetwork,
    kAudioChimeBlockedByQuietHours,
};

struct AudioChimePolicyInput {
    bool low_battery_mode = false;
    bool ota_updating = false;
    bool wifi_radio_on = false;
    bool setup_portal_active = false;
    bool ota_checking = false;
    bool enforce_quiet_hours = true;
    bool all_day_enabled = false;
    int hour = 0;
};

constexpr int kHourlyChimeStartHour = 7;
constexpr int kHourlyChimeEndHour = 22;

constexpr bool audio_playback_blocked_by_system(bool low_battery_mode,
                                                bool ota_updating)
{
    return low_battery_mode || ota_updating;
}

constexpr AudioChimeDecision audio_hourly_chime_decision(const AudioChimePolicyInput &input)
{
    if (audio_playback_blocked_by_system(input.low_battery_mode,
                                         input.ota_updating)) {
        return kAudioChimeBlockedBySystem;
    }
    if (input.wifi_radio_on || input.setup_portal_active || input.ota_checking) {
        return kAudioChimeBlockedByNetwork;
    }
    if (input.enforce_quiet_hours &&
        !input.all_day_enabled &&
        (input.hour < kHourlyChimeStartHour || input.hour > kHourlyChimeEndHour)) {
        return kAudioChimeBlockedByQuietHours;
    }
    return kAudioChimePlay;
}

static_assert(kHourlyChimeStartHour >= 0 && kHourlyChimeStartHour < 24,
              "hourly chime start hour must be in 0..23");
static_assert(kHourlyChimeEndHour >= 0 && kHourlyChimeEndHour < 24,
              "hourly chime end hour must be in 0..23");
static_assert(kHourlyChimeStartHour <= kHourlyChimeEndHour,
              "hourly chime active window must not wrap midnight");
