// 私有持有显示与 I2C 硬件对象，保持原有静态构造参数和应用全生命周期。
#include "app_hardware.h"

#include "app_display_config.h"

namespace {
DisplayPort s_display(12, 11, 5, 40, 41, kDisplayWidth, kDisplayHeight);
I2cMasterBus s_i2c(14, 13, 0);
} // namespace

DisplayPort &app_display()
{
    return s_display;
}

I2cMasterBus &app_i2c()
{
    return s_i2c;
}
