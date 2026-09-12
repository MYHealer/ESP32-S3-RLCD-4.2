// 验证通用 UI 图标在多个编译单元之间共享同一份只读对象。
#include "ui_icons.h"

#include <assert.h>
#include <stddef.h>

const uint8_t *ui_icon_owner_a(size_t index);

int main()
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
    for (size_t index = 0; index < kIconCount; ++index) {
        assert(ui_icon_owner_a(index) == kIcons[index]);
    }
    assert(ui_icon_owner_a(kIconCount) == nullptr);
    return 0;
}
