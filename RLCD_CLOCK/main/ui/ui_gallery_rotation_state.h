// 声明图片时钟自定义图库切换周期及其线程安全运行态。
#pragma once

#include <stdint.h>

enum GalleryRotationPeriod : uint8_t {
    kGalleryRotation30Minutes = 0,
    kGalleryRotation1Hour,
    kGalleryRotation6Hours,
    kGalleryRotation12Hours,
    kGalleryRotation24Hours,
    kGalleryRotationPeriodCount,
};

inline constexpr uint8_t kDefaultGalleryRotationPeriod = kGalleryRotation24Hours;

uint8_t normalize_gallery_rotation_period(uint8_t period);
uint8_t next_gallery_rotation_period(uint8_t period);
int gallery_rotation_period_minutes(uint8_t period);
const char *gallery_rotation_period_label(uint8_t period);
int effective_gallery_rotation_minutes(uint8_t period, int custom_image_count);
const char *effective_gallery_rotation_label(uint8_t period, int custom_image_count);

uint8_t gallery_rotation_period_load();
