// 实现图片时钟图库索引映射、已绘制来源缓存和自定义图片有限重试。
#include "ui_gallery_selection.h"

namespace {
constexpr int kWeekdayCount = 7;
constexpr int kFirstSupportedYear = 1970;
constexpr int kLastSupportedYear = 9999;

constexpr bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

constexpr int days_in_month(int year, int month)
{
    constexpr int kMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    return month == 2 && is_leap_year(year) ? 29 : kMonthDays[month - 1];
}

constexpr bool valid_date(int year, int month, int day)
{
    return year >= kFirstSupportedYear && year <= kLastSupportedYear &&
           day >= 1 && day <= days_in_month(year, month);
}

// Gregorian civil date to a monotonic day number. Only the relative sequence
// matters here, so the epoch offset is intentionally omitted.
int64_t civil_day_number(int year, int month, int day)
{
    year -= month <= 2;
    const int era = year / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned shifted_month = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned day_of_year = (153U * shifted_month + 2U) / 5U +
                                 static_cast<unsigned>(day - 1);
    const unsigned day_of_era = year_of_era * 365U + year_of_era / 4U -
                                year_of_era / 100U + day_of_year;
    return static_cast<int64_t>(era) * 146097LL + day_of_era;
}

int positive_mod(int64_t value, int divisor)
{
    int remainder = static_cast<int>(value % divisor);
    return remainder < 0 ? remainder + divisor : remainder;
}

constexpr bool valid_time(int hour, int minute)
{
    return hour >= 0 && hour < 24 && minute >= 0 && minute < 60;
}

constexpr bool valid_rotation_minutes(int minutes)
{
    return minutes == 30 || minutes == 60 || minutes == 360 ||
           minutes == 720 || minutes == 1440;
}
}

bool gallery_image_selection_for_date(int year,
                                      int month,
                                      int day,
                                      int weekday,
                                      int custom_image_count,
                                      int builtin_image_count,
                                      GalleryImageSelection *selection)
{
    return gallery_image_selection_for_time(year,
                                            month,
                                            day,
                                            0,
                                            0,
                                            weekday,
                                            custom_image_count,
                                            builtin_image_count,
                                            1440,
                                            selection);
}

bool gallery_image_selection_for_time(int year,
                                      int month,
                                      int day,
                                      int hour,
                                      int minute,
                                      int weekday,
                                      int custom_image_count,
                                      int builtin_image_count,
                                      int custom_rotation_minutes,
                                      GalleryImageSelection *selection)
{
    if (!selection || !valid_date(year, month, day) ||
        !valid_time(hour, minute) || !valid_rotation_minutes(custom_rotation_minutes) ||
        weekday < 0 || weekday >= kWeekdayCount ||
        custom_image_count < 0 || builtin_image_count <= 0) {
        return false;
    }
    selection->uses_custom_gallery = custom_image_count > 0;
    selection->builtin_index = weekday % builtin_image_count;
    if (selection->uses_custom_gallery) {
        const int slots_per_day = 1440 / custom_rotation_minutes;
        const int minute_of_day = hour * 60 + minute;
        const int64_t slot = civil_day_number(year, month, day) * slots_per_day +
                             minute_of_day / custom_rotation_minutes;
        selection->image_index = positive_mod(slot, custom_image_count);
    } else {
        selection->image_index = selection->builtin_index;
    }
    return true;
}

void gallery_image_render_cache_reset(GalleryImageRenderCache *cache)
{
    if (!cache) {
        return;
    }
    cache->selected_index = -1;
    cache->builtin_index = -1;
    cache->used_custom_image = false;
}

bool gallery_image_render_cache_matches(const GalleryImageRenderCache &cache,
                                        const GalleryImageSelection &selection,
                                        bool used_custom_image)
{
    if (used_custom_image) {
        return selection.uses_custom_gallery &&
               cache.used_custom_image &&
               cache.selected_index == selection.image_index;
    }
    return !cache.used_custom_image &&
           cache.builtin_index == selection.builtin_index &&
           (!selection.uses_custom_gallery ||
            cache.selected_index == selection.image_index);
}

void gallery_image_render_cache_record(GalleryImageRenderCache *cache,
                                       const GalleryImageSelection &selection,
                                       bool used_custom_image)
{
    if (!cache) {
        return;
    }
    cache->selected_index = selection.image_index;
    cache->builtin_index = selection.builtin_index;
    cache->used_custom_image =
        selection.uses_custom_gallery && used_custom_image;
}

void gallery_custom_image_retry_reset(GalleryCustomImageRetryState *state)
{
    if (!state) {
        return;
    }
    state->image_index = -1;
    state->last_attempt_minute_key = UINT32_MAX;
    state->attempts = 0;
}

bool gallery_custom_image_retry_pending(const GalleryCustomImageRetryState &state,
                                        int image_index)
{
    return image_index >= 0 &&
           state.image_index == image_index &&
           state.attempts > 0;
}

bool gallery_custom_image_should_attempt(const GalleryCustomImageRetryState &state,
                                         int image_index,
                                         uint32_t minute_key)
{
    if (image_index < 0) {
        return false;
    }
    if (state.image_index != image_index || state.attempts == 0) {
        return true;
    }
    return state.attempts < kGalleryCustomImageMaxReadAttempts &&
           state.last_attempt_minute_key != minute_key;
}

void gallery_custom_image_record_result(GalleryCustomImageRetryState *state,
                                        int image_index,
                                        uint32_t minute_key,
                                        bool success)
{
    if (!state) {
        return;
    }
    if (success || image_index < 0) {
        gallery_custom_image_retry_reset(state);
        return;
    }
    if (state->image_index != image_index) {
        state->image_index = image_index;
        state->attempts = 0;
    }
    state->last_attempt_minute_key = minute_key;
    if (state->attempts < kGalleryCustomImageMaxReadAttempts) {
        ++state->attempts;
    }
}
