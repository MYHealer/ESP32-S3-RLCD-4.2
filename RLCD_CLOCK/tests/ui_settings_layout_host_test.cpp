// 验证设置页共享布局坐标、容量和越界保护保持稳定。
#include "ui_settings_layout.h"

#include <assert.h>

int main()
{
    using namespace ui_settings_layout;

    static_assert(kSettingsListRowCount == 8,
                  "settings list must cover every secondary slot");
    static_assert(kSettingsGridCapacity == 8,
                  "settings compact grid must remain four rows by two columns");

    assert(kSettingsPrimaryX == 12);
    assert(kSettingsPrimaryW == 112);
    assert(kSettingsSecondaryX == 150);
    assert(kSettingsSecondaryW == 228);
    assert(kSettingsSecondaryH == 30);
    assert(kSettingsGridSwitchLabelRightPadding >=
           kSettingsGridColW - kSettingsGridSwitchDotXOffset +
               kSettingsGridSwitchLabelDotGap);

    const GridCell first = settings_grid_cell(0);
    const GridCell second = settings_grid_cell(1);
    const GridCell final = settings_grid_cell(7);
    assert(first.x == 150 && first.y == 66);
    assert(second.x == 267 && second.y == 66);
    assert(final.x == 267 && final.y == 183);

    assert(!settings_grid_index_valid(-1));
    assert(!settings_grid_index_valid(kSettingsGridCapacity));
    const GridCell invalid = settings_grid_cell(kSettingsGridCapacity);
    assert(invalid.x == -1 && invalid.y == -1);
    return 0;
}
