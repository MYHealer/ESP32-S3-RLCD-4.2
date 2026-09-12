// 定义图片时钟固件与 SDL 预览共用的布局和块状数字。
#pragma once

#include "clock_gallery_contract.h"

#include <cstddef>
#include <cstdint>

namespace ui_gallery_layout {

inline constexpr int kGalleryImageCanvasX = 20;
inline constexpr int kGalleryImageCanvasY = 66;
inline constexpr int kGalleryDividerX = 252;
inline constexpr int kGalleryDividerY = 70;
inline constexpr int kGalleryDividerW = 3;
inline constexpr int kGalleryDividerH =
    kGalleryImageCanvasY + CLOCK_GALLERY_IMAGE_HEIGHT - kGalleryDividerY;
inline constexpr int kGalleryTimeCanvasX = 268;
inline constexpr int kGalleryTimeCanvasY = 66;
inline constexpr int kGalleryTimeCanvasW = 112;
inline constexpr int kGalleryTimeCanvasH = 198;
inline constexpr int kGallerySayingLabelX = 18;
inline constexpr int kGallerySayingLabelY = 275;
inline constexpr int kGallerySayingLabelW = 364;
inline constexpr int kGallerySayingLabelH = 24;

inline constexpr int kGalleryBlockDigitRows = 7;
inline constexpr int kGalleryBlockDigitCols = 5;
inline constexpr int kGalleryBlockDigitScale = 10;
inline constexpr int kGalleryBlockDigitGap = 8;
inline constexpr int kGalleryBlockDigitW =
    kGalleryBlockDigitCols * kGalleryBlockDigitScale;
inline constexpr int kGalleryBlockDigitH =
    kGalleryBlockDigitRows * kGalleryBlockDigitScale;
inline constexpr int kGalleryBlockNumberW =
    kGalleryBlockDigitW * 2 + kGalleryBlockDigitGap;
inline constexpr int kGalleryDecimalBase = 10;
inline constexpr int kGalleryBlockDigitCount = 10;
inline constexpr uint8_t kGalleryTensDigitMask = 1U << 0;
inline constexpr uint8_t kGalleryOnesDigitMask = 1U << 1;
inline constexpr uint8_t kGalleryBothDigitsMask =
    kGalleryTensDigitMask | kGalleryOnesDigitMask;
inline constexpr int kGalleryTimeHourY = 15;
inline constexpr int kGalleryTimeMinuteY = 116;

inline constexpr const char *kGalleryBlockDigits[kGalleryBlockDigitCount]
                                                        [kGalleryBlockDigitRows] = {
    {"11111", "10001", "10011", "10101", "11001", "10001", "11111"},
    {"00100", "01100", "00100", "00100", "00100", "00100", "01110"},
    {"11110", "00001", "00001", "11110", "10000", "10000", "11111"},
    {"11110", "00001", "00001", "01110", "00001", "00001", "11110"},
    {"10010", "10010", "10010", "11111", "00010", "00010", "00010"},
    {"11111", "10000", "10000", "11110", "00001", "00001", "11110"},
    {"01111", "10000", "10000", "11110", "10001", "10001", "01110"},
    {"11111", "00001", "00010", "00100", "01000", "01000", "01000"},
    {"01110", "10001", "10001", "01110", "10001", "10001", "01110"},
    {"01110", "10001", "10001", "01111", "00001", "00001", "11110"},
};

constexpr int gallery_block_digit_x(int digit_index)
{
    return (kGalleryTimeCanvasW - kGalleryBlockNumberW) / 2 +
           digit_index * (kGalleryBlockDigitW + kGalleryBlockDigitGap);
}

constexpr int gallery_saying_rule_length(int width,int text_width) {
    if(width<=0 || text_width<=0 || text_width>=width) return 0;
    const int length=(width-text_width)/2-10;
    return length>=4?length:0;
}
static_assert(gallery_saying_rule_length(364,176)==84,"short text keeps balanced rules");
static_assert(gallery_saying_rule_length(364,340)==0,"long text hides decoration");
static_assert(gallery_saying_rule_length(364,400)==0,"overflow must not draw rules");
static_assert(gallery_saying_rule_length(364,0)==0,"empty text hides decoration");

constexpr uint8_t gallery_changed_digit_mask(int previous, int current)
{
    if (current < 0 ||
        current >= kGalleryDecimalBase * kGalleryDecimalBase) {
        return 0;
    }
    if (previous < 0 ||
        previous >= kGalleryDecimalBase * kGalleryDecimalBase) {
        return kGalleryBothDigitsMask;
    }
    return static_cast<uint8_t>(
        ((previous / kGalleryDecimalBase) !=
                 (current / kGalleryDecimalBase)
             ? kGalleryTensDigitMask
             : 0U) |
        ((previous % kGalleryDecimalBase) !=
                 (current % kGalleryDecimalBase)
             ? kGalleryOnesDigitMask
             : 0U));
}

static_assert(kGalleryDividerW > 0 && kGalleryDividerH > 0,
              "gallery divider size must be positive");
static_assert(kGalleryDividerY + kGalleryDividerH ==
                  kGalleryImageCanvasY + CLOCK_GALLERY_IMAGE_HEIGHT,
              "gallery divider bottom must align with the image canvas");
static_assert(kGalleryTimeCanvasW > 0 && kGalleryTimeCanvasH > 0,
              "gallery time canvas dimensions must be positive");
static_assert(kGallerySayingLabelW > 0 && kGallerySayingLabelH > 0,
              "gallery saying label size must be positive");
static_assert(kGalleryBlockDigitRows > 0 && kGalleryBlockDigitCols > 0,
              "gallery block digit grid must be positive");
static_assert(kGalleryBlockDigitScale > 1,
              "gallery block digit scale must leave a visible gap");
static_assert(kGalleryBlockDigitGap >= 0,
              "gallery block digit gap must not be negative");
static_assert(kGalleryBlockNumberW <= kGalleryTimeCanvasW,
              "gallery block number must fit the time canvas");
static_assert(kGalleryTimeHourY >= 0 && kGalleryTimeMinuteY >= 0,
              "gallery block digit Y must not be negative");
static_assert(kGalleryTimeHourY + kGalleryBlockDigitH <=
                  kGalleryTimeCanvasH,
              "gallery hour digits must fit the time canvas");
static_assert(kGalleryTimeMinuteY + kGalleryBlockDigitH <=
                  kGalleryTimeCanvasH,
              "gallery minute digits must fit the time canvas");
static_assert(kGalleryTimeHourY + kGalleryBlockDigitH <
                  kGalleryTimeMinuteY,
              "gallery hour and minute regions must not overlap");
static_assert(gallery_block_digit_x(0) >= 0,
              "gallery first digit must fit the time canvas");
static_assert(gallery_block_digit_x(0) + kGalleryBlockDigitW <=
                  gallery_block_digit_x(1),
              "gallery digit dirty regions must not overlap");
static_assert(gallery_block_digit_x(1) + kGalleryBlockDigitW <=
                  kGalleryTimeCanvasW,
              "gallery second digit must fit the time canvas");
static_assert(gallery_changed_digit_mask(-1, 0) ==
                  kGalleryBothDigitsMask,
              "gallery initial draw must update both digits");
static_assert(gallery_changed_digit_mask(34, 35) ==
                  kGalleryOnesDigitMask,
              "gallery ordinary minute change must update one digit");
static_assert(gallery_changed_digit_mask(39, 40) ==
                  kGalleryBothDigitsMask,
              "gallery decimal rollover must update both digits");
static_assert(gallery_changed_digit_mask(10, 10) == 0,
              "gallery unchanged value must not redraw digits");

} // namespace ui_gallery_layout
