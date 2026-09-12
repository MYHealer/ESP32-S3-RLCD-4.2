// 提供图片时钟图库选择、已绘制来源缓存和自定义图片读取重试的纯逻辑。
#pragma once

#include <stdint.h>

struct GalleryImageSelection {
    int image_index;
    int builtin_index;
    bool uses_custom_gallery;
};

struct GalleryImageRenderCache {
    int selected_index;
    int builtin_index;
    bool used_custom_image;
};

struct GalleryCustomImageRetryState {
    uint32_t last_attempt_minute_key;
    int image_index;
    uint8_t attempts;
};

constexpr uint8_t kGalleryCustomImageMaxReadAttempts = 3;

bool gallery_image_selection_for_date(int year,
                                      int month,
                                      int day,
                                      int weekday,
                                      int custom_image_count,
                                      int builtin_image_count,
                                      GalleryImageSelection *selection);

bool gallery_image_selection_for_time(int year,
                                      int month,
                                      int day,
                                      int hour,
                                      int minute,
                                      int weekday,
                                      int custom_image_count,
                                      int builtin_image_count,
                                      int custom_rotation_minutes,
                                      GalleryImageSelection *selection);

void gallery_image_render_cache_reset(GalleryImageRenderCache *cache);
bool gallery_image_render_cache_matches(const GalleryImageRenderCache &cache,
                                        const GalleryImageSelection &selection,
                                        bool used_custom_image);
void gallery_image_render_cache_record(GalleryImageRenderCache *cache,
                                       const GalleryImageSelection &selection,
                                       bool used_custom_image);

void gallery_custom_image_retry_reset(GalleryCustomImageRetryState *state);
bool gallery_custom_image_retry_pending(const GalleryCustomImageRetryState &state,
                                        int image_index);
bool gallery_custom_image_should_attempt(const GalleryCustomImageRetryState &state,
                                         int image_index,
                                         uint32_t minute_key);
void gallery_custom_image_record_result(GalleryCustomImageRetryState *state,
                                        int image_index,
                                        uint32_t minute_key,
                                        bool success);
