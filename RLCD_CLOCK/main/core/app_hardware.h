// 提供应用唯一的显示与 I2C 硬件对象，只暴露稳定引用而不公开可替换全局量。
#pragma once

#include "display_bsp.h"
#include "i2c_bsp.h"

DisplayPort &app_display();
I2cMasterBus &app_i2c();
