// 验证图片时钟内置图库按星期、自定义图库按配置时段轮换及回退索引。
#include "ui_gallery_selection.h"
#include "ui_gallery_rotation_state_internal.h"

#include <assert.h>

namespace {
void assert_same_slot_then_advance(int before_hour,
                                   int before_minute,
                                   int boundary_hour,
                                   int boundary_minute,
                                   int rotation_minutes)
{
    GalleryImageSelection selection = {};
    assert(gallery_image_selection_for_time(2026,
                                            7,
                                            12,
                                            before_hour,
                                            before_minute,
                                            0,
                                            127,
                                            7,
                                            rotation_minutes,
                                            &selection));
    const int before_index = selection.image_index;
    assert(gallery_image_selection_for_time(2026,
                                            7,
                                            12,
                                            boundary_hour,
                                            boundary_minute,
                                            0,
                                            127,
                                            7,
                                            rotation_minutes,
                                            &selection));
    assert(selection.image_index == (before_index + 1) % 127);
}
} // namespace

int main()
{
    assert(normalize_gallery_rotation_period(99) == kGalleryRotation24Hours);
    assert(next_gallery_rotation_period(kGalleryRotation30Minutes) == kGalleryRotation1Hour);
    assert(next_gallery_rotation_period(kGalleryRotation24Hours) == kGalleryRotation30Minutes);
    assert(gallery_rotation_period_minutes(kGalleryRotation6Hours) == 360);
    assert(effective_gallery_rotation_minutes(kGalleryRotation30Minutes, 0) == 1440);
    assert(effective_gallery_rotation_minutes(kGalleryRotation30Minutes, 3) == 30);
    assert(gallery_rotation_period_load() == kGalleryRotation24Hours);
    gallery_rotation_period_store(kGalleryRotation12Hours);
    assert(gallery_rotation_period_load() == kGalleryRotation12Hours);

    GalleryImageSelection selection = {};

    assert(gallery_image_selection_for_date(2026, 7, 12, 0, 0, 7, &selection));
    assert(selection.image_index == 0);
    assert(selection.builtin_index == 0);
    assert(!selection.uses_custom_gallery);

    assert(gallery_image_selection_for_date(2026, 7, 13, 1, 0, 7, &selection));
    assert(selection.image_index == 1);
    assert(selection.builtin_index == 1);
    assert(!selection.uses_custom_gallery);

    assert(gallery_image_selection_for_date(2026, 7, 12, 0, 24, 7, &selection));
    int sunday_custom_index = selection.image_index;
    assert(selection.builtin_index == 0);
    assert(selection.uses_custom_gallery);

    assert(gallery_image_selection_for_date(2026, 7, 13, 1, 24, 7, &selection));
    assert(selection.image_index == (sunday_custom_index + 1) % 24);
    assert(selection.builtin_index == 1);
    assert(selection.uses_custom_gallery);

    assert(gallery_image_selection_for_date(2026, 12, 31, 4, 7, 7, &selection));
    int year_end_index = selection.image_index;
    assert(gallery_image_selection_for_date(2027, 1, 1, 5, 7, 7, &selection));
    assert(selection.image_index == (year_end_index + 1) % 7);

    assert(gallery_image_selection_for_time(2026, 7, 12, 10, 0, 0, 24, 7, 30, &selection));
    int half_hour_index = selection.image_index;
    assert(gallery_image_selection_for_time(2026, 7, 12, 10, 29, 0, 24, 7, 30, &selection));
    assert(selection.image_index == half_hour_index);
    assert(gallery_image_selection_for_time(2026, 7, 12, 10, 30, 0, 24, 7, 30, &selection));
    assert(selection.image_index == (half_hour_index + 1) % 24);

    assert_same_slot_then_advance(0, 29, 0, 30, 30);
    assert_same_slot_then_advance(10, 59, 11, 0, 60);
    assert_same_slot_then_advance(5, 59, 6, 0, 360);
    assert_same_slot_then_advance(11, 59, 12, 0, 720);

    assert(gallery_image_selection_for_time(2026, 7, 12, 23, 30, 0, 24, 7, 30, &selection));
    int final_day_slot = selection.image_index;
    assert(gallery_image_selection_for_time(2026, 7, 13, 0, 0, 1, 24, 7, 30, &selection));
    assert(selection.image_index == (final_day_slot + 1) % 24);

    assert(gallery_image_selection_for_time(2026, 7, 12, 10, 30, 0, 0, 7, 30, &selection));
    assert(selection.image_index == 0);
    assert(!selection.uses_custom_gallery);

    assert(!gallery_image_selection_for_date(2026, 2, 29, 0, 0, 7, &selection));
    assert(!gallery_image_selection_for_date(2026, 7, 12, -1, 0, 7, &selection));
    assert(!gallery_image_selection_for_date(2026, 7, 12, 7, 0, 7, &selection));
    assert(!gallery_image_selection_for_date(2026, 7, 12, 0, -1, 7, &selection));
    assert(!gallery_image_selection_for_date(2026, 7, 12, 0, 0, 0, &selection));
    assert(!gallery_image_selection_for_date(2026, 7, 12, 0, 0, 7, nullptr));
    assert(!gallery_image_selection_for_time(2026, 7, 12, 24, 0, 0, 2, 7, 30, &selection));
    assert(!gallery_image_selection_for_time(2026, 7, 12, 10, 60, 0, 2, 7, 30, &selection));
    assert(!gallery_image_selection_for_time(2026, 7, 12, 10, 0, 0, 2, 7, 45, &selection));

    GalleryImageRenderCache render_cache = {};
    gallery_image_render_cache_reset(&render_cache);
    assert(!gallery_image_render_cache_matches(render_cache, selection, false));

    selection.image_index = 2;
    selection.builtin_index = 4;
    selection.uses_custom_gallery = true;
    gallery_image_render_cache_record(&render_cache, selection, false);
    assert(gallery_image_render_cache_matches(render_cache, selection, false));
    assert(!gallery_image_render_cache_matches(render_cache, selection, true));
    selection.image_index = 3;
    assert(!gallery_image_render_cache_matches(render_cache, selection, false));
    selection.image_index = 2;
    selection.builtin_index = 5;
    assert(!gallery_image_render_cache_matches(render_cache, selection, false));
    selection.builtin_index = 4;
    gallery_image_render_cache_record(&render_cache, selection, true);
    assert(gallery_image_render_cache_matches(render_cache, selection, true));
    assert(!gallery_image_render_cache_matches(render_cache, selection, false));

    GalleryCustomImageRetryState retry_state = {};
    gallery_custom_image_retry_reset(&retry_state);
    assert(!gallery_custom_image_retry_pending(retry_state, 2));
    assert(gallery_custom_image_should_attempt(retry_state, 2, 100));

    gallery_custom_image_record_result(&retry_state, 2, 100, false);
    assert(gallery_custom_image_retry_pending(retry_state, 2));
    assert(!gallery_custom_image_should_attempt(retry_state, 2, 100));
    assert(gallery_custom_image_should_attempt(retry_state, 2, 101));

    gallery_custom_image_record_result(&retry_state, 2, 101, false);
    assert(gallery_custom_image_should_attempt(retry_state, 2, 102));
    gallery_custom_image_record_result(&retry_state, 2, 102, false);
    assert(!gallery_custom_image_should_attempt(retry_state, 2, 103));

    assert(gallery_custom_image_should_attempt(retry_state, 3, 103));
    gallery_custom_image_record_result(&retry_state, 3, 103, false);
    assert(gallery_custom_image_retry_pending(retry_state, 3));
    gallery_custom_image_record_result(&retry_state, 3, 104, true);
    assert(!gallery_custom_image_retry_pending(retry_state, 3));
    assert(gallery_custom_image_should_attempt(retry_state, 3, 104));
    return 0;
}
