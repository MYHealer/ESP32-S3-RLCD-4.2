// 验证 Wi-Fi 驱动初始化失败后的安全重试与永久阻断状态转换。
#include "wifi_driver_init_policy.h"

#include <assert.h>

int main()
{
    assert(wifi_driver_init_retry_allowed(WifiDriverInitState::kRetryable));
    assert(!wifi_driver_init_ready(WifiDriverInitState::kRetryable));

    const WifiDriverInitState ready =
        wifi_driver_init_state_after_attempt(true, false);
    assert(wifi_driver_init_ready(ready));
    assert(!wifi_driver_init_retry_allowed(ready));

    const WifiDriverInitState retryable =
        wifi_driver_init_state_after_attempt(false, true);
    assert(!wifi_driver_init_ready(retryable));
    assert(wifi_driver_init_retry_allowed(retryable));

    const WifiDriverInitState blocked =
        wifi_driver_init_state_after_attempt(false, false);
    assert(!wifi_driver_init_ready(blocked));
    assert(!wifi_driver_init_retry_allowed(blocked));
    return 0;
}
