// 声明每日文字联网更新入口，避免调度层依赖文本缓存容量。
#pragma once

bool perform_daily_saying_update();
