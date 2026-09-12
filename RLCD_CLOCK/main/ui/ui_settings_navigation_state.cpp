// 使用单个 32 位原子量发布设置页的完整导航状态。
#include "ui_settings_navigation_state_internal.h"

#include <atomic>
#include <cstdint>

namespace {
constexpr uint32_t kFocusSecondaryBit = 1U << 0;
constexpr uint32_t kPageToggleModeBit = 1U << 1;
constexpr uint32_t kPageOrderModeBit = 1U << 2;
constexpr uint32_t kPrimarySelectionShift = 3;
constexpr uint32_t kSelectionShift = 11;
constexpr uint32_t kPageOrderSelectionShift = 19;
constexpr uint32_t kFieldMask = 0xffU;

std::atomic<uint32_t> s_settings_navigation_state{0};

constexpr uint32_t encode_field(int value, uint32_t shift)
{
    return (static_cast<uint32_t>(value) & kFieldMask) << shift;
}

constexpr int decode_field(uint32_t packed, uint32_t shift)
{
    return static_cast<int>((packed >> shift) & kFieldMask);
}

constexpr uint32_t encode_settings_navigation(const SettingsNavigationSnapshot &state)
{
    return (state.focus_secondary ? kFocusSecondaryBit : 0U) |
           (state.page_toggle_mode ? kPageToggleModeBit : 0U) |
           (state.page_order_mode ? kPageOrderModeBit : 0U) |
           encode_field(state.primary_selection, kPrimarySelectionShift) |
           encode_field(state.selection, kSelectionShift) |
           encode_field(state.page_order_selection, kPageOrderSelectionShift);
}

static_assert(kPageOrderSelectionShift + 8 <= 32,
              "settings navigation snapshot must fit in one 32-bit atomic");
static_assert((kFocusSecondaryBit | kPageToggleModeBit | kPageOrderModeBit) <
                  (1U << kPrimarySelectionShift),
              "settings navigation flags must not overlap selection fields");
} // namespace

SettingsNavigationSnapshot settings_navigation_snapshot()
{
    const uint32_t packed = s_settings_navigation_state.load(std::memory_order_acquire);
    SettingsNavigationSnapshot state;
    state.focus_secondary = (packed & kFocusSecondaryBit) != 0;
    state.page_toggle_mode = (packed & kPageToggleModeBit) != 0;
    state.page_order_mode = (packed & kPageOrderModeBit) != 0;
    state.primary_selection = decode_field(packed, kPrimarySelectionShift);
    state.selection = decode_field(packed, kSelectionShift);
    state.page_order_selection = decode_field(packed, kPageOrderSelectionShift);
    return state;
}

void settings_navigation_store(const SettingsNavigationSnapshot &state)
{
    s_settings_navigation_state.store(encode_settings_navigation(state),
                                      std::memory_order_release);
}
