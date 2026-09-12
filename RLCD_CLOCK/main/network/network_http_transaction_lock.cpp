// 独占 HTTPS/WSS 全局事务锁的应用期静态所有权和串行化接口。
#include "network_http_transaction_lock_internal.h"

#include "app_metadata.h"
#include "scoped_semaphore_lock.h"

#include "esp_log.h"

namespace {
constexpr const char *kHttpTransactionMutexCreateFailedLog =
    "http transaction mutex create failed";
StaticTaskMutex s_http_transaction_mutex;
} // namespace

bool init_network_http_transaction_lock()
{
    if (s_http_transaction_mutex.init()) {
        return true;
    }
    ESP_LOGE(TAG, "%s", kHttpTransactionMutexCreateFailedLog);
    return false;
}

bool acquire_network_http_transaction_lock(TickType_t timeout)
{
    SemaphoreHandle_t mutex = s_http_transaction_mutex.handle();
    return mutex && xSemaphoreTake(mutex, timeout) == pdTRUE;
}

void release_network_http_transaction_lock()
{
    SemaphoreHandle_t mutex = s_http_transaction_mutex.handle();
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}
