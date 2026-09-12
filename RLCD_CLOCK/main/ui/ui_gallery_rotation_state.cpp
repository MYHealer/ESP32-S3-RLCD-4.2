// 实现图片时钟切换周期映射、内置图库 24 小时约束和原子运行态。
#include "ui_gallery_rotation_state_internal.h"

#include <atomic>

namespace {
constexpr int kGalleryRotationMinutes[] = {30, 60, 360, 720, 1440};
constexpr const char *kGalleryRotationLabels[] = {"30m", "1h", "6h", "12h", "24h"};

std::atomic<uint8_t> s_gallery_rotation_period{kDefaultGalleryRotationPeriod};

static_assert(sizeof(kGalleryRotationMinutes) / sizeof(kGalleryRotationMinutes[0]) ==
                  kGalleryRotationPeriodCount,
              "gallery rotation minute table must match period count");
static_assert(sizeof(kGalleryRotationLabels) / sizeof(kGalleryRotationLabels[0]) ==
                  kGalleryRotationPeriodCount,
              "gallery rotation label table must match period count");
static_assert(1440 % kGalleryRotationMinutes[kGalleryRotation30Minutes] == 0 &&
                  1440 % kGalleryRotationMinutes[kGalleryRotation1Hour] == 0 &&
                  1440 % kGalleryRotationMinutes[kGalleryRotation6Hours] == 0 &&
                  1440 % kGalleryRotationMinutes[kGalleryRotation12Hours] == 0 &&
                  1440 % kGalleryRotationMinutes[kGalleryRotation24Hours] == 0,
              "gallery rotation periods must align to local-day boundaries");
} // namespace

uint8_t normalize_gallery_rotation_period(uint8_t period)
{
    return period < kGalleryRotationPeriodCount ? period : kDefaultGalleryRotationPeriod;
}

uint8_t next_gallery_rotation_period(uint8_t period)
{
    return static_cast<uint8_t>(
        (normalize_gallery_rotation_period(period) + 1U) % kGalleryRotationPeriodCount);
}

int gallery_rotation_period_minutes(uint8_t period)
{
    return kGalleryRotationMinutes[normalize_gallery_rotation_period(period)];
}

const char *gallery_rotation_period_label(uint8_t period)
{
    return kGalleryRotationLabels[normalize_gallery_rotation_period(period)];
}

int effective_gallery_rotation_minutes(uint8_t period, int custom_image_count)
{
    return custom_image_count > 0
               ? gallery_rotation_period_minutes(period)
               : gallery_rotation_period_minutes(kGalleryRotation24Hours);
}

const char *effective_gallery_rotation_label(uint8_t period, int custom_image_count)
{
    return custom_image_count > 0
               ? gallery_rotation_period_label(period)
               : gallery_rotation_period_label(kGalleryRotation24Hours);
}

uint8_t gallery_rotation_period_load()
{
    return normalize_gallery_rotation_period(
        s_gallery_rotation_period.load(std::memory_order_acquire));
}

void gallery_rotation_period_store(uint8_t period)
{
    s_gallery_rotation_period.store(normalize_gallery_rotation_period(period),
                                    std::memory_order_release);
}
