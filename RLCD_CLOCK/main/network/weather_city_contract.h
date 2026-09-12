// 定义手动天气城市在配网、运行态、NVS 和上位机资源之间共享的容量契约。
#pragma once

inline constexpr int kManualWeatherCityLen = 32;

static_assert(kManualWeatherCityLen > 1,
              "manual weather city buffer must fit text and NUL");
