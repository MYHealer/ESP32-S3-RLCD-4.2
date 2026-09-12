// 验证整点提醒阻断顺序、静音时段边界和全天模式。
#include "audio_chime_policy.h"

#include <assert.h>

int main()
{
    AudioChimePolicyInput input = {};
    input.hour = kHourlyChimeStartHour;
    assert(audio_hourly_chime_decision(input) == kAudioChimePlay);
    input.hour = kHourlyChimeEndHour;
    assert(audio_hourly_chime_decision(input) == kAudioChimePlay);

    input.hour = kHourlyChimeStartHour - 1;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedByQuietHours);
    input.hour = kHourlyChimeEndHour + 1;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedByQuietHours);

    input.all_day_enabled = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimePlay);
    input.all_day_enabled = false;
    input.enforce_quiet_hours = false;
    assert(audio_hourly_chime_decision(input) == kAudioChimePlay);

    input.enforce_quiet_hours = true;
    input.hour = 12;
    input.wifi_radio_on = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedByNetwork);
    input.wifi_radio_on = false;
    input.setup_portal_active = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedByNetwork);
    input.setup_portal_active = false;
    input.ota_checking = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedByNetwork);

    input.low_battery_mode = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedBySystem);
    input.low_battery_mode = false;
    input.ota_updating = true;
    assert(audio_hourly_chime_decision(input) == kAudioChimeBlockedBySystem);
    assert(audio_playback_blocked_by_system(false, false) == false);
    assert(audio_playback_blocked_by_system(true, false) == true);
    assert(audio_playback_blocked_by_system(false, true) == true);
    return 0;
}
