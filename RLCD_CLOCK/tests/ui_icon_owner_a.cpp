// 返回第一个编译单元看到的通用 UI 图标地址。
#include "ui_icons.h"

#include <stddef.h>

const uint8_t *ui_icon_owner_a(size_t index)
{
    static constexpr const uint8_t *kIcons[] = {
        low_battery_icon_bits,
        warning_icon_bits,
        chime_status_icon_bits,
        wifi_status_icon_bits,
        alarm_status_icon_bits,
        trend_up_icon_bits,
        trend_down_icon_bits,
        temp_icon_bits,
        humi_icon_bits,
    };
    constexpr size_t kIconCount = sizeof(kIcons) / sizeof(kIcons[0]);
    return index < kIconCount ? kIcons[index] : nullptr;
}
