// 管理工作页名称、启用状态、页面特征和用户自定义顺序。
#include "ui_work_page_catalog_internal.h"

#include "active_work_page_state_internal.h"
#include "app_constexpr.h"
#include "scoped_semaphore_lock.h"
#include "ui_work_page_order_policy.h"

#include <atomic>
#include <string.h>

namespace {
constexpr int kFirstWorkPage = kWorkPageWeatherClock;
constexpr int kFallbackWorkPage = kWorkPageWeatherClock;
constexpr uint8_t kWorkPageTraitRequiresNetwork = 1U << 0;
constexpr uint8_t kWorkPageTraitLowRefreshIdle = 1U << 1;
constexpr uint8_t kWorkPageTraitWeatherData = 1U << 2;
constexpr uint8_t kWorkPageTraitDailySaying = 1U << 3;
constexpr uint8_t kWorkPageTraitExtendedWeatherData = 1U << 4;
constexpr uint8_t kKnownWorkPageTraits =
    kWorkPageTraitRequiresNetwork |
    kWorkPageTraitLowRefreshIdle |
    kWorkPageTraitWeatherData |
    kWorkPageTraitDailySaying |
    kWorkPageTraitExtendedWeatherData;

struct WorkPageDescriptor {
    uint8_t page;
    const char *name;
    uint8_t traits;
};

constexpr WorkPageDescriptor kWorkPageDescriptors[kWorkPageCount] = {
    {kWorkPageWeatherClock,
     "天气时钟",
     kWorkPageTraitRequiresNetwork | kWorkPageTraitWeatherData},
    {kWorkPageGallery,
     "图片时钟",
     kWorkPageTraitRequiresNetwork | kWorkPageTraitLowRefreshIdle |
         kWorkPageTraitDailySaying},
    {kWorkPageWeatherBoard,
     "天气看板",
     kWorkPageTraitRequiresNetwork | kWorkPageTraitLowRefreshIdle |
         kWorkPageTraitWeatherData | kWorkPageTraitExtendedWeatherData},
    {kWorkPageFlipClock, "温湿时钟", 0},
    {kWorkPageCalendar, "日历", kWorkPageTraitLowRefreshIdle},
    {kWorkPageHistory, "温湿历史", kWorkPageTraitLowRefreshIdle},
    {kWorkPageXiaozhiAI, "小智AI", kWorkPageTraitRequiresNetwork},
    {kWorkPageAggregateClock, "聚合时钟", kWorkPageTraitRequiresNetwork |
         kWorkPageTraitWeatherData | kWorkPageTraitExtendedWeatherData},
};

constexpr uint8_t kDefaultWorkPageOrder[kWorkPageCount] = {
    kWorkPageWeatherClock,
    kWorkPageGallery,
    kWorkPageWeatherBoard,
    kWorkPageFlipClock,
    kWorkPageCalendar,
    kWorkPageHistory,
    kWorkPageXiaozhiAI,
    kWorkPageAggregateClock,
};
StaticTaskMutex s_work_page_order_mutex;
uint8_t s_work_page_order[kWorkPageCount] = {
    kWorkPageWeatherClock,
    kWorkPageGallery,
    kWorkPageWeatherBoard,
    kWorkPageFlipClock,
    kWorkPageCalendar,
    kWorkPageHistory,
    kWorkPageXiaozhiAI,
    kWorkPageAggregateClock,
};
constexpr const char *kUnknownWorkPageName = "未知页面";

constexpr uint8_t work_page_mask(int page)
{
    return static_cast<uint8_t>(1U << page);
}

constexpr uint8_t kAllWorkPageMask = static_cast<uint8_t>((1U << kWorkPageCount) - 1U);
std::atomic<uint8_t> s_work_page_enabled_mask{kAllWorkPageMask};

constexpr uint8_t work_page_mask_for_trait(uint8_t trait)
{
    uint8_t mask = 0;
    for (const WorkPageDescriptor &descriptor : kWorkPageDescriptors) {
        if ((descriptor.traits & trait) != 0) {
            mask = static_cast<uint8_t>(mask | work_page_mask(descriptor.page));
        }
    }
    return mask;
}

constexpr uint8_t kNetworkWorkPageMask =
    work_page_mask_for_trait(kWorkPageTraitRequiresNetwork);
constexpr uint8_t kLowRefreshIdleWorkPageMask =
    work_page_mask_for_trait(kWorkPageTraitLowRefreshIdle);
constexpr uint8_t kWeatherDataWorkPageMask =
    work_page_mask_for_trait(kWorkPageTraitWeatherData);
constexpr uint8_t kDailySayingWorkPageMask =
    work_page_mask_for_trait(kWorkPageTraitDailySaying);
constexpr uint8_t kExtendedWeatherDataWorkPageMask =
    work_page_mask_for_trait(kWorkPageTraitExtendedWeatherData);
constexpr uint8_t kLocalWorkPageMask = static_cast<uint8_t>(~kNetworkWorkPageMask) & kAllWorkPageMask;

constexpr bool work_page_descriptors_are_indexed_by_id()
{
    for (size_t i = 0; i < array_count(kWorkPageDescriptors); ++i) {
        if (kWorkPageDescriptors[i].page != i) {
            return false;
        }
    }
    return true;
}

template <size_t N>
constexpr bool page_list_covers_each_work_page_once(const uint8_t (&pages)[N])
{
    if (N != kWorkPageCount) {
        return false;
    }
    for (int page = kFirstWorkPage; page < kWorkPageCount; ++page) {
        int hits = 0;
        for (uint8_t candidate : pages) {
            if (candidate == page) {
                ++hits;
            }
        }
        if (hits != 1) {
            return false;
        }
    }
    return true;
}

constexpr bool work_page_descriptor_names_are_nonempty()
{
    for (const WorkPageDescriptor &descriptor : kWorkPageDescriptors) {
        if (!cstr_nonempty(descriptor.name)) {
            return false;
        }
    }
    return true;
}

constexpr bool work_page_descriptor_traits_are_valid()
{
    constexpr uint8_t kNetworkDataTraits =
        kWorkPageTraitWeatherData | kWorkPageTraitDailySaying |
        kWorkPageTraitExtendedWeatherData;
    for (const WorkPageDescriptor &descriptor : kWorkPageDescriptors) {
        if ((descriptor.traits & ~kKnownWorkPageTraits) != 0) {
            return false;
        }
        if ((descriptor.traits & kNetworkDataTraits) != 0 &&
            (descriptor.traits & kWorkPageTraitRequiresNetwork) == 0) {
            return false;
        }
        if ((descriptor.traits & kWorkPageTraitExtendedWeatherData) != 0 &&
            (descriptor.traits & kWorkPageTraitWeatherData) == 0) {
            return false;
        }
    }
    return true;
}

constexpr bool work_page_has_trait(int page, uint8_t trait)
{
    return page >= kFirstWorkPage &&
           page < kWorkPageCount &&
           (kWorkPageDescriptors[page].traits & trait) != 0;
}

bool copy_normalized_work_page_order(uint8_t *order,
                                     size_t order_size,
                                     uint8_t page_mask)
{
    if (!order || order_size != sizeof(s_work_page_order)) {
        return false;
    }
    ScopedSemaphoreLock lock(s_work_page_order_mutex.handle());
    if (!lock) {
        return false;
    }
    work_page_order_policy::normalize(s_work_page_order,
                                      sizeof(s_work_page_order),
                                      page_mask,
                                      kDefaultWorkPageOrder,
                                      sizeof(kDefaultWorkPageOrder));
    memcpy(order, s_work_page_order, sizeof(s_work_page_order));
    return true;
}

static_assert(kFirstWorkPage == 0, "work page ids must start at zero");
static_assert(kFallbackWorkPage == kWorkPageWeatherClock, "work page order fallback must remain weather clock");
static_assert(kWorkPageCount > 0, "there must be at least one work page");
static_assert(kWorkPageCount <= static_cast<int>(sizeof(uint8_t) * 8),
              "work page enabled mask is stored as uint8_t");
static_assert(kLocalWorkPageMask != 0, "offline mode requires at least one local work page");
static_assert((kLocalWorkPageMask & kNetworkWorkPageMask) == 0,
              "local and network work page masks must not overlap");
static_assert(kLowRefreshIdleWorkPageMask != 0,
              "low-refresh idle page mask must not be empty");
static_assert((kLowRefreshIdleWorkPageMask & ~kAllWorkPageMask) == 0,
              "low-refresh idle page mask must only contain work pages");
static_assert((kWeatherDataWorkPageMask & ~kNetworkWorkPageMask) == 0,
              "weather-data pages must require network access");
static_assert((kDailySayingWorkPageMask & ~kNetworkWorkPageMask) == 0,
              "daily-saying pages must require network access");
static_assert((kExtendedWeatherDataWorkPageMask & ~kWeatherDataWorkPageMask) == 0,
              "extended-weather pages must consume basic weather data");
static_assert(array_count(kDefaultWorkPageOrder) == kWorkPageCount,
              "default work page order must cover every work page");
static_assert(array_count(kWorkPageDescriptors) == kDisplaySettingsPageItemCount,
              "display setting page mapping must match the settings item count");
static_assert(array_count(kWorkPageDescriptors) == kWorkPageCount,
              "work page descriptors must cover every work page");
static_assert(work_page_descriptor_names_are_nonempty(),
              "work page descriptor names must be non-empty");
static_assert(cstr_nonempty(kUnknownWorkPageName), "unknown work page name must be non-empty");
static_assert(work_page_descriptors_are_indexed_by_id(),
              "work page descriptors must follow the contiguous work page ids");
static_assert(work_page_descriptor_traits_are_valid(),
              "network data traits must only belong to network pages");
static_assert(page_list_covers_each_work_page_once(kDefaultWorkPageOrder),
              "default work page order must include every work page exactly once");
}

bool work_page_catalog_init()
{
    return s_work_page_order_mutex.init();
}

bool is_work_page_enabled(int page)
{
    if (!work_page_order_policy::is_work_page(page)) {
        return false;
    }
    return (work_page_enabled_mask_load() & work_page_mask(page)) != 0;
}

uint8_t work_page_enabled_mask_load()
{
    return s_work_page_enabled_mask.load(std::memory_order_acquire);
}

void work_page_enabled_mask_store(uint8_t page_mask)
{
    s_work_page_enabled_mask.store(page_mask, std::memory_order_release);
}

bool work_page_requires_network(int page)
{
    return work_page_order_policy::is_work_page(page) &&
           work_page_has_trait(page, kWorkPageTraitRequiresNetwork);
}

bool work_page_uses_low_refresh_idle(int page)
{
    return work_page_order_policy::is_work_page(page) &&
           work_page_has_trait(page, kWorkPageTraitLowRefreshIdle);
}

WorkPageDataRequirements work_page_data_requirements(int page)
{
    return {
        work_page_has_trait(page, kWorkPageTraitWeatherData),
        work_page_has_trait(page, kWorkPageTraitExtendedWeatherData),
        work_page_has_trait(page, kWorkPageTraitDailySaying),
        page == kWorkPageWeatherBoard,
    };
}

WorkPageDataRequirements enabled_work_page_data_requirements(uint8_t page_mask)
{
    page_mask = static_cast<uint8_t>(page_mask & kAllWorkPageMask);
    return {
        (page_mask & kWeatherDataWorkPageMask) != 0,
        (page_mask & kExtendedWeatherDataWorkPageMask) != 0,
        (page_mask & kDailySayingWorkPageMask) != 0,
        (page_mask & work_page_mask(kWorkPageWeatherBoard)) != 0,
    };
}

uint8_t normalize_work_page_enabled_mask(uint8_t page_mask)
{
    page_mask &= kAllWorkPageMask;
    if (page_mask == 0) {
        return kAllWorkPageMask;
    }
    if (!work_page_order_policy::mask_has_valid_home(page_mask)) {
        page_mask |= work_page_mask(kWorkPageWeatherClock);
    }
    return page_mask;
}

uint8_t work_page_mask_for_offline_mode(uint8_t page_mask)
{
    uint8_t local_mask = static_cast<uint8_t>(page_mask & kLocalWorkPageMask);
    if (local_mask != 0) {
        return local_mask;
    }
    uint8_t order[kWorkPageCount] = {};
    if (work_page_order_copy(order, sizeof(order)) &&
        work_page_order_policy::order_is_valid(order, sizeof(order))) {
        for (uint8_t page : order) {
            if (!work_page_requires_network(page)) {
                return work_page_mask(page);
            }
        }
    }
    return work_page_mask(kWorkPageFlipClock);
}

const char *work_page_name(int page)
{
    if (!work_page_order_policy::is_work_page(page)) {
        return kUnknownWorkPageName;
    }
    return kWorkPageDescriptors[page].name;
}

int first_enabled_work_page()
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    uint8_t order[kWorkPageCount] = {};
    if (!copy_normalized_work_page_order(order, sizeof(order), page_mask)) {
        return kFallbackWorkPage;
    }
    int index = work_page_order_policy::first_enabled_index(
        order, sizeof(order), page_mask);
    return work_page_order_policy::index_found(index)
               ? order[index]
               : kFallbackWorkPage;
}

void reset_work_page_order()
{
    static_assert(sizeof(kDefaultWorkPageOrder) == sizeof(s_work_page_order),
                  "default work page order storage must match runtime order storage");
    ScopedSemaphoreLock lock(s_work_page_order_mutex.handle());
    if (!lock) {
        return;
    }
    memcpy(s_work_page_order, kDefaultWorkPageOrder, sizeof(s_work_page_order));
}

void normalize_work_page_order()
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    ScopedSemaphoreLock lock(s_work_page_order_mutex.handle());
    if (!lock) {
        return;
    }
    work_page_order_policy::normalize(s_work_page_order,
                                      sizeof(s_work_page_order),
                                      page_mask,
                                      kDefaultWorkPageOrder,
                                      sizeof(kDefaultWorkPageOrder));
}

bool work_page_mask_has_valid_home(uint8_t page_mask)
{
    return work_page_order_policy::mask_has_valid_home(page_mask);
}

bool work_page_order_copy(uint8_t *order, size_t order_size)
{
    if (!order || order_size != sizeof(s_work_page_order)) {
        return false;
    }
    ScopedSemaphoreLock lock(s_work_page_order_mutex.handle());
    if (!lock) {
        return false;
    }
    memcpy(order, s_work_page_order, sizeof(s_work_page_order));
    return true;
}

bool work_page_order_normalize_and_copy(uint8_t *order, size_t order_size)
{
    return copy_normalized_work_page_order(
        order, order_size, work_page_enabled_mask_load());
}

void work_page_order_replace(const uint8_t *order, size_t order_size)
{
    uint8_t replacement[kWorkPageCount] = {};
    if (work_page_order_policy::order_is_valid(order, order_size)) {
        memcpy(replacement, order, sizeof(replacement));
    } else {
        memcpy(replacement, kDefaultWorkPageOrder, sizeof(replacement));
    }
    work_page_order_policy::normalize(replacement,
                                      sizeof(replacement),
                                      work_page_enabled_mask_load(),
                                      kDefaultWorkPageOrder,
                                      sizeof(kDefaultWorkPageOrder));
    ScopedSemaphoreLock lock(s_work_page_order_mutex.handle());
    if (!lock) {
        return;
    }
    memcpy(s_work_page_order, replacement, sizeof(s_work_page_order));
}

bool work_page_order_swapped_copy_preserving_home(int first_index,
                                                  int second_index,
                                                  uint8_t *order,
                                                  size_t order_size)
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    if (!copy_normalized_work_page_order(order, order_size, page_mask)) {
        return false;
    }
    return work_page_order_policy::swap_entries_preserving_home(
        order,
        order_size,
        page_mask,
        first_index,
        second_index);
}

int next_enabled_work_page(int current_page)
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    uint8_t order[kWorkPageCount] = {};
    if (!copy_normalized_work_page_order(order, sizeof(order), page_mask)) {
        return kFallbackWorkPage;
    }
    if (!work_page_order_policy::is_work_page(current_page)) {
        int first_index = work_page_order_policy::first_enabled_index(
            order, sizeof(order), page_mask);
        current_page = work_page_order_policy::index_found(first_index)
                           ? order[first_index]
                           : kFallbackWorkPage;
    }
    int current_index = work_page_order_policy::index_of(
        order, sizeof(order), current_page);
    int next_index = work_page_order_policy::next_enabled_index(
        order, sizeof(order), page_mask, current_index);
    return work_page_order_policy::index_found(next_index)
               ? order[next_index]
               : kFallbackWorkPage;
}

int first_enabled_work_page_order_index()
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    uint8_t order[kWorkPageCount] = {};
    if (!copy_normalized_work_page_order(order, sizeof(order), page_mask)) {
        return 0;
    }
    int first_index = work_page_order_policy::first_enabled_index(
        order, sizeof(order), page_mask);
    return work_page_order_policy::index_found(first_index) ? first_index : 0;
}

int next_enabled_work_page_order_index(int current_order_index)
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    uint8_t order[kWorkPageCount] = {};
    if (!copy_normalized_work_page_order(order, sizeof(order), page_mask)) {
        return 0;
    }
    current_order_index = work_page_order_policy::valid_enabled_index(
        order, sizeof(order), page_mask, current_order_index);
    int next_index = work_page_order_policy::next_enabled_index(
        order, sizeof(order), page_mask, current_order_index);
    return work_page_order_policy::index_found(next_index)
               ? next_index
               : work_page_order_policy::valid_enabled_index(
                     order, sizeof(order), page_mask, -1);
}

int valid_enabled_work_page_order_index(int current_order_index)
{
    const uint8_t page_mask = work_page_enabled_mask_load();
    uint8_t order[kWorkPageCount] = {};
    if (!copy_normalized_work_page_order(order, sizeof(order), page_mask)) {
        return 0;
    }
    return work_page_order_policy::valid_enabled_index(
        order, sizeof(order), page_mask, current_order_index);
}

void ensure_active_work_page_enabled()
{
    if (!is_work_page_enabled(active_work_page_load())) {
        active_work_page_store(first_enabled_work_page());
    }
}
