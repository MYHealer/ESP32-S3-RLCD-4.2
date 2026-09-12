// 验证 Wi-Fi 射频运行状态按启动和停止结果安全发布。
#include "wifi_radio_state_internal.h"

#include <cassert>
#include <cstdio>

int main()
{
    assert(!wifi_radio_on_load());

    wifi_radio_on_store(true);
    assert(wifi_radio_on_load());

    wifi_radio_on_store(false);
    assert(!wifi_radio_on_load());

    std::puts("Wi-Fi radio state host tests passed");
    return 0;
}
