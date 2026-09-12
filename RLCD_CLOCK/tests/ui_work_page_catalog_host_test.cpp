// 验证工作页名称、启用掩码、离线筛选和自定义顺序规则。
#include "ui_work_page_catalog.h"
#include "ui_work_page_catalog_internal.h"
#include "ui_work_page_order_policy.h"

#include "active_work_page_state_internal.h"

#include <assert.h>
#include <atomic>
#include <string.h>
#include <thread>

namespace {

constexpr uint8_t page_bit(int page)
{
    return static_cast<uint8_t>(1U << page);
}

void expect_default_order()
{
    const uint8_t expected[kWorkPageCount] = {
        kWorkPageWeatherClock,
        kWorkPageGallery,
        kWorkPageWeatherBoard,
        kWorkPageFlipClock,
        kWorkPageCalendar,
        kWorkPageHistory,
        kWorkPageXiaozhiAI,
        kWorkPageAggregateClock,
    };
    uint8_t actual[kWorkPageCount] = {};
    assert(work_page_order_copy(actual, sizeof(actual)));
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
}

void expect_order_policy_boundaries()
{
    const uint8_t default_order[kWorkPageCount] = {
        kWorkPageWeatherClock,
        kWorkPageGallery,
        kWorkPageWeatherBoard,
        kWorkPageFlipClock,
        kWorkPageCalendar,
        kWorkPageHistory,
        kWorkPageXiaozhiAI,
        kWorkPageAggregateClock,
    };
    assert(!work_page_order_policy::order_is_valid(nullptr, kWorkPageCount));
    assert(!work_page_order_policy::order_is_valid(default_order,
                                                    kWorkPageCount - 1));
    assert(work_page_order_policy::order_is_valid(default_order,
                                                   sizeof(default_order)));
    assert(work_page_order_policy::index_of(default_order,
                                             sizeof(default_order),
                                             kWorkPageCalendar) == 4);
    assert(work_page_order_policy::index_of(nullptr,
                                             kWorkPageCount,
                                             kWorkPageCalendar) ==
           work_page_order_policy::kInvalidIndex);

    uint8_t invalid_order[kWorkPageCount] = {};
    assert(!work_page_order_policy::order_is_valid(invalid_order,
                                                    sizeof(invalid_order)));
    assert(work_page_order_policy::normalize(invalid_order,
                                              sizeof(invalid_order),
                                              page_bit(kWorkPageCalendar),
                                              default_order,
                                              sizeof(default_order)));
    assert(memcmp(invalid_order, default_order, sizeof(default_order)) == 0);

    uint8_t xiaozhi_first[kWorkPageCount] = {
        kWorkPageXiaozhiAI,
        kWorkPageHistory,
        kWorkPageCalendar,
        kWorkPageFlipClock,
        kWorkPageWeatherBoard,
        kWorkPageGallery,
        kWorkPageWeatherClock,
        kWorkPageAggregateClock,
    };
    const uint8_t enabled = page_bit(kWorkPageXiaozhiAI) |
                            page_bit(kWorkPageHistory) |
                            page_bit(kWorkPageCalendar);
    assert(work_page_order_policy::normalize(xiaozhi_first,
                                              sizeof(xiaozhi_first),
                                              enabled,
                                              default_order,
                                              sizeof(default_order)));
    assert(xiaozhi_first[0] == kWorkPageHistory);
    assert(xiaozhi_first[1] == kWorkPageXiaozhiAI);
    assert(work_page_order_policy::first_enabled_index(xiaozhi_first,
                                                        sizeof(xiaozhi_first),
                                                        enabled) == 0);
    assert(work_page_order_policy::next_enabled_index(xiaozhi_first,
                                                       sizeof(xiaozhi_first),
                                                       enabled,
                                                       0) == 1);
    assert(work_page_order_policy::valid_enabled_index(xiaozhi_first,
                                                        sizeof(xiaozhi_first),
                                                        enabled,
                                                        3) == 0);
    assert(work_page_order_policy::mask_has_valid_home(enabled));
    assert(work_page_order_policy::order_has_valid_home(xiaozhi_first,
                                                         sizeof(xiaozhi_first),
                                                         enabled));
    assert(!work_page_order_policy::mask_has_valid_home(
        page_bit(kWorkPageXiaozhiAI)));
    assert(!work_page_order_policy::order_has_valid_home(
        xiaozhi_first,
        sizeof(xiaozhi_first),
        page_bit(kWorkPageXiaozhiAI)));
    uint8_t swap_candidate[kWorkPageCount] = {};
    memcpy(swap_candidate, default_order, sizeof(swap_candidate));
    assert(work_page_order_policy::swap_entries_preserving_home(
        swap_candidate,
        sizeof(swap_candidate),
        static_cast<uint8_t>((1U << kWorkPageCount) - 1U),
        0,
        1));
    assert(swap_candidate[0] == kWorkPageGallery);
    assert(swap_candidate[1] == kWorkPageWeatherClock);
    const uint8_t preserved_candidate[kWorkPageCount] = {
        kWorkPageWeatherClock,
        kWorkPageGallery,
        kWorkPageWeatherBoard,
        kWorkPageFlipClock,
        kWorkPageCalendar,
        kWorkPageHistory,
        kWorkPageXiaozhiAI,
        kWorkPageAggregateClock,
    };
    memcpy(swap_candidate, preserved_candidate, sizeof(swap_candidate));
    assert(!work_page_order_policy::swap_entries_preserving_home(
        swap_candidate,
        sizeof(swap_candidate),
        page_bit(kWorkPageWeatherClock) | page_bit(kWorkPageXiaozhiAI),
        0,
        kWorkPageXiaozhiAI));
    assert(memcmp(swap_candidate,
                  preserved_candidate,
                  sizeof(swap_candidate)) == 0);
    assert(!work_page_order_policy::normalize(invalid_order,
                                               sizeof(invalid_order),
                                               enabled,
                                               nullptr,
                                               kWorkPageCount));
}

} // namespace

int main()
{
    expect_order_policy_boundaries();
    uint8_t unavailable[kWorkPageCount] = {};
    assert(!work_page_order_copy(unavailable, sizeof(unavailable)));
    assert(!work_page_order_normalize_and_copy(unavailable, sizeof(unavailable)));
    assert(!work_page_order_swapped_copy_preserving_home(
        0, 1, unavailable, sizeof(unavailable)));
    assert(work_page_catalog_init());
    assert(work_page_catalog_init());
    reset_work_page_order();
    expect_default_order();
    work_page_enabled_mask_store(static_cast<uint8_t>((1U << kWorkPageCount) - 1U));
    uint8_t swapped_order[kWorkPageCount] = {};
    assert(work_page_order_swapped_copy_preserving_home(
        0, 1, swapped_order, sizeof(swapped_order)));
    assert(swapped_order[0] == kWorkPageGallery);
    assert(swapped_order[1] == kWorkPageWeatherClock);
    expect_default_order();

    const char *const expected_names[kWorkPageCount] = {
        "天气时钟",
        "图片时钟",
        "天气看板",
        "温湿时钟",
        "日历",
        "温湿历史",
        "小智AI", "聚合时钟",
    };
    const WorkPageDataRequirements expected_data[kWorkPageCount] = {
        {true, false, false},
        {false, false, true},
        {true, true, false},
        {false, false, false},
        {false, false, false},
        {false, false, false},
        {false, false, false},
        {true, true, false},
    };
    for (int page = 0; page < kWorkPageCount; ++page) {
        assert(strcmp(work_page_name(page), expected_names[page]) == 0);
        const WorkPageDataRequirements actual =
            work_page_data_requirements(page);
        assert(actual.weather == expected_data[page].weather);
        assert(actual.extended_weather ==
               expected_data[page].extended_weather);
        assert(actual.daily_saying == expected_data[page].daily_saying);
    }
    assert(strcmp(work_page_name(-1), "未知页面") == 0);

    assert(work_page_requires_network(kWorkPageWeatherClock));
    assert(work_page_requires_network(kWorkPageGallery));
    assert(work_page_requires_network(kWorkPageWeatherBoard));
    assert(work_page_requires_network(kWorkPageXiaozhiAI));
    assert(!work_page_requires_network(kWorkPageFlipClock));
    assert(!work_page_requires_network(kWorkPageCalendar));
    assert(!work_page_requires_network(kWorkPageHistory));
    assert(!work_page_requires_network(-1));

    assert(!work_page_uses_low_refresh_idle(kWorkPageWeatherClock));
    assert(work_page_uses_low_refresh_idle(kWorkPageGallery));
    assert(work_page_uses_low_refresh_idle(kWorkPageWeatherBoard));
    assert(!work_page_uses_low_refresh_idle(kWorkPageFlipClock));
    assert(work_page_uses_low_refresh_idle(kWorkPageCalendar));
    assert(work_page_uses_low_refresh_idle(kWorkPageHistory));
    assert(!work_page_uses_low_refresh_idle(kWorkPageXiaozhiAI));
    assert(!work_page_uses_low_refresh_idle(-1));
    assert(!work_page_data_requirements(-1).weather);
    assert(!work_page_data_requirements(-1).extended_weather);
    assert(!work_page_data_requirements(-1).daily_saying);

    const uint8_t all_pages = static_cast<uint8_t>((1U << kWorkPageCount) - 1U);
    WorkPageDataRequirements enabled_data =
        enabled_work_page_data_requirements(all_pages);
    assert(enabled_data.weather);
    assert(enabled_data.extended_weather);
    assert(enabled_data.daily_saying);
    enabled_data = enabled_work_page_data_requirements(
        page_bit(kWorkPageWeatherBoard));
    assert(enabled_data.weather);
    assert(enabled_data.extended_weather);
    assert(!enabled_data.daily_saying);
    enabled_data = enabled_work_page_data_requirements(
        page_bit(kWorkPageWeatherClock));
    assert(enabled_data.weather);
    assert(!enabled_data.extended_weather);
    assert(!enabled_data.daily_saying);
    enabled_data = enabled_work_page_data_requirements(
        page_bit(kWorkPageGallery));
    assert(!enabled_data.weather);
    assert(!enabled_data.extended_weather);
    assert(enabled_data.daily_saying);
    enabled_data = enabled_work_page_data_requirements(
        page_bit(kWorkPageCalendar));
    assert(!enabled_data.weather);
    assert(!enabled_data.extended_weather);
    assert(!enabled_data.daily_saying);
    assert(normalize_work_page_enabled_mask(all_pages) == all_pages);
    assert(normalize_work_page_enabled_mask(0) == all_pages);
    assert(normalize_work_page_enabled_mask(0x80) == 0x80);
    assert(normalize_work_page_enabled_mask(page_bit(kWorkPageXiaozhiAI)) ==
           (page_bit(kWorkPageWeatherClock) | page_bit(kWorkPageXiaozhiAI)));
    assert(normalize_work_page_enabled_mask(page_bit(kWorkPageCalendar) |
                                            page_bit(kWorkPageXiaozhiAI)) ==
           (page_bit(kWorkPageCalendar) | page_bit(kWorkPageXiaozhiAI)));

    const uint8_t local_pages = page_bit(kWorkPageFlipClock) |
                                page_bit(kWorkPageCalendar);
    assert(work_page_mask_for_offline_mode(work_page_enabled_mask_load()) ==
           (page_bit(kWorkPageFlipClock) |
            page_bit(kWorkPageCalendar) |
            page_bit(kWorkPageHistory)));
    assert(work_page_mask_for_offline_mode(local_pages |
                                           page_bit(kWorkPageWeatherClock)) ==
           local_pages);
    assert(work_page_mask_for_offline_mode(page_bit(kWorkPageWeatherClock)) ==
           page_bit(kWorkPageFlipClock));

    const uint8_t xiaozhi_first[kWorkPageCount] = {
        kWorkPageXiaozhiAI,
        kWorkPageHistory,
        kWorkPageCalendar,
        kWorkPageFlipClock,
        kWorkPageWeatherBoard,
        kWorkPageGallery,
        kWorkPageWeatherClock,
        kWorkPageAggregateClock,
    };
    work_page_order_replace(xiaozhi_first, sizeof(xiaozhi_first));
    uint8_t normalized[kWorkPageCount] = {};
    assert(work_page_order_normalize_and_copy(normalized, sizeof(normalized)));
    assert(normalized[0] == kWorkPageHistory);
    assert(normalized[1] == kWorkPageXiaozhiAI);
    assert(first_enabled_work_page() == kWorkPageHistory);

    work_page_enabled_mask_store(page_bit(kWorkPageXiaozhiAI));
    normalize_work_page_order();
    assert(!work_page_mask_has_valid_home(work_page_enabled_mask_load()));

    work_page_enabled_mask_store(page_bit(kWorkPageWeatherClock) |
                                 page_bit(kWorkPageCalendar) |
                                 page_bit(kWorkPageHistory));
    reset_work_page_order();
    assert(first_enabled_work_page() == kWorkPageWeatherClock);
    assert(next_enabled_work_page(kWorkPageWeatherClock) == kWorkPageCalendar);
    assert(next_enabled_work_page(kWorkPageCalendar) == kWorkPageHistory);
    assert(next_enabled_work_page(kWorkPageHistory) == kWorkPageWeatherClock);

    active_work_page_store(kWorkPageGallery);
    ensure_active_work_page_enabled();
    assert(active_work_page_load() == kWorkPageWeatherClock);

    const uint8_t invalid_order[kWorkPageCount] = {
        kWorkPageWeatherClock,
        kWorkPageWeatherClock,
        kWorkPageAggregateClock,
    };
    work_page_order_replace(invalid_order, sizeof(invalid_order));
    expect_default_order();

    const uint8_t default_order[kWorkPageCount] = {
        kWorkPageWeatherClock,
        kWorkPageGallery,
        kWorkPageWeatherBoard,
        kWorkPageFlipClock,
        kWorkPageCalendar,
        kWorkPageHistory,
        kWorkPageXiaozhiAI,
        kWorkPageAggregateClock,
    };
    const uint8_t alternate_order[kWorkPageCount] = {
        kWorkPageHistory,
        kWorkPageCalendar,
        kWorkPageFlipClock,
        kWorkPageWeatherBoard,
        kWorkPageGallery,
        kWorkPageWeatherClock,
        kWorkPageXiaozhiAI,
        kWorkPageAggregateClock,
    };
    work_page_enabled_mask_store(all_pages);
    std::atomic<bool> writer_done{false};
    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            const uint8_t *order = (i & 1) ? default_order : alternate_order;
            work_page_order_replace(order, kWorkPageCount);
        }
        writer_done.store(true, std::memory_order_release);
    });
    do {
        uint8_t snapshot[kWorkPageCount] = {};
        assert(work_page_order_copy(snapshot, sizeof(snapshot)));
        assert(memcmp(snapshot, default_order, sizeof(snapshot)) == 0 ||
               memcmp(snapshot, alternate_order, sizeof(snapshot)) == 0);
    } while (!writer_done.load(std::memory_order_acquire));
    writer.join();
    return 0;
}
