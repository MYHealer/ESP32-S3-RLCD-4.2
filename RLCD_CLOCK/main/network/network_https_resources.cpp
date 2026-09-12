// 实现 HTTPS 期间的显示 DMA 保护和内存余量采样。
#include "network_https_resources.h"
#include "network_https_resources_internal.h"

#include "network_sync_schedule.h"

#include "display_bsp.h"
#include "esp_heap_caps.h"

NetworkDisplayDmaGuard::NetworkDisplayDmaGuard(bool active) : active_(active)
{
    if (active_) {
        Display_AcquireDmaConservativeMode();
    }
}

NetworkDisplayDmaGuard::~NetworkDisplayDmaGuard()
{
    if (active_) {
        Display_ReleaseDmaConservativeMode();
    }
}

NetworkHttpsMemorySnapshot capture_network_https_memory_snapshot()
{
    NetworkHttpsMemorySnapshot snapshot;
    snapshot.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    snapshot.dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    snapshot.sufficient = network_boot_https_memory_sufficient(snapshot.internal_free,
                                                               snapshot.internal_largest,
                                                               snapshot.dma_largest);
    return snapshot;
}
