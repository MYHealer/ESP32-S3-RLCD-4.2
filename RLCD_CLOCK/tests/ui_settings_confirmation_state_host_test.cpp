// 验证设置页三类二次确认状态互不干扰并支持单项和全部清理。
#include "ui_settings_confirmation_state_internal.h"

#include <cassert>

int main()
{
    constexpr SettingsConfirmation kInvalidConfirmation =
        static_cast<SettingsConfirmation>(99);

    settings_confirmation_clear_all();
    assert(!settings_confirmation_pending(SettingsConfirmation::kFactoryReset));
    assert(!settings_confirmation_pending(SettingsConfirmation::kOfflineDisable));
    assert(!settings_confirmation_pending(SettingsConfirmation::kWeatherCityClear));

    settings_confirmation_request(SettingsConfirmation::kFactoryReset);
    settings_confirmation_request(SettingsConfirmation::kOfflineDisable);
    settings_confirmation_request(SettingsConfirmation::kWeatherCityClear);
    assert(settings_confirmation_pending(SettingsConfirmation::kFactoryReset));
    assert(settings_confirmation_pending(SettingsConfirmation::kOfflineDisable));
    assert(settings_confirmation_pending(SettingsConfirmation::kWeatherCityClear));

    settings_confirmation_clear(SettingsConfirmation::kOfflineDisable);
    assert(settings_confirmation_pending(SettingsConfirmation::kFactoryReset));
    assert(!settings_confirmation_pending(SettingsConfirmation::kOfflineDisable));
    assert(settings_confirmation_pending(SettingsConfirmation::kWeatherCityClear));

    settings_confirmation_request(kInvalidConfirmation);
    settings_confirmation_clear(kInvalidConfirmation);
    assert(!settings_confirmation_pending(kInvalidConfirmation));

    settings_confirmation_clear_all();
    assert(!settings_confirmation_pending(SettingsConfirmation::kFactoryReset));
    assert(!settings_confirmation_pending(SettingsConfirmation::kOfflineDisable));
    assert(!settings_confirmation_pending(SettingsConfirmation::kWeatherCityClear));
    return 0;
}
