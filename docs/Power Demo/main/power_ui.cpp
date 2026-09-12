// 单页面 UI：直接写 RLCD 1-bit 缓冲，避免创建 LVGL 对象、任务和动画。
#include "power_ui.h"

#include "calendar_lunar.h"
#include "display_bsp.h"
#include "dseg_digits.h"
#include "flip_sensor_icons.h"
#include "power_fonts.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kDisplayWidth = 400;
constexpr int kDisplayHeight = 300;
constexpr int kCardCount = 3;
constexpr int kCardX[kCardCount] = {18, 144, 270};
constexpr int kCardY = 66;
constexpr int kCardW = 112;
constexpr int kCardH = 112;
constexpr int kCardRadius = 8;

constexpr int kTopSeparatorX = 18;
constexpr int kTopSeparatorY = 54;
constexpr int kTopSeparatorW = 364;
constexpr int kTopSeparatorH = 4;

constexpr int kSensorPanelX = 18;
constexpr int kSensorPanelY = 198;
constexpr int kSensorPanelW = 238;
constexpr int kSensorPanelH = 88;
constexpr int kSensorPanelRadius = 18;
constexpr int kDatePanelX = 270;
constexpr int kDatePanelY = 198;
constexpr int kDatePanelW = 112;
constexpr int kDatePanelH = 88;
constexpr int kDatePanelRadius = 18;

constexpr int kProgressX = 20;
constexpr int kProgressY = 59;
constexpr int kProgressSegments = 60;
constexpr int kProgressSegmentW = 5;
constexpr int kProgressSegmentH = 3;
constexpr int kProgressStride = 6;

constexpr int kBatteryFrameX = 20;
constexpr int kBatteryFrameY = 17;
constexpr int kBatteryFrameW = 34;
constexpr int kBatteryFrameH = 16;
constexpr int kBatteryTipX = 55;
constexpr int kBatteryTipY = 22;
constexpr int kBatterySegmentCount = 5;

constexpr int kChimeX = 64;
constexpr int kChimeY = 15;
constexpr int kChimeW = 19;
constexpr int kChimeH = 19;
constexpr int kChimeBytesPerRow = 3;
constexpr uint8_t kChimeBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0xE0, 0x00,
    0x01, 0xE0, 0x80, 0x03, 0x24, 0xC0, 0x3F, 0x26, 0x40, 0x70, 0x26, 0x60,
    0x60, 0x22, 0x60, 0x60, 0x22, 0x60, 0x60, 0x22, 0x60, 0x60, 0x22, 0x60,
    0x70, 0x26, 0x60, 0x3F, 0x26, 0x40, 0x03, 0x24, 0xC0, 0x01, 0xE0, 0x80,
    0x00, 0xE0, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
};

constexpr const char *kWeekdayNames[] = {
    "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六",
};

struct RefreshRange {
    int x1;
    int x2;
};

struct DsegPairLayout {
    int first_origin_x;
    int second_origin_x;
};

constexpr int dseg_scaled_position(int value, int scale_num, int scale_den)
{
    return scale_den > 0 ? (value * scale_num) / scale_den : 0;
}

constexpr int dseg_scaled_size(int value, int scale_num, int scale_den)
{
    return scale_den > 0
               ? (value * scale_num + scale_den - 1) / scale_den
               : 0;
}

constexpr DsegPairLayout centered_dseg_pair_layout(int container_width,
                                                    int scale_num,
                                                    int scale_den,
                                                    int first_x_offset,
                                                    int first_width,
                                                    int first_x_advance,
                                                    int second_x_offset,
                                                    int second_width)
{
    if (scale_num <= 0 || scale_den <= 0) {
        return {0, 0};
    }
    const int second_origin =
        dseg_scaled_position(first_x_advance, scale_num, scale_den);
    const int first_left =
        dseg_scaled_position(first_x_offset, scale_num, scale_den);
    const int second_left = second_origin +
                            dseg_scaled_position(second_x_offset,
                                                 scale_num,
                                                 scale_den);
    const int left = first_left < second_left ? first_left : second_left;
    const int first_right = first_left +
                            dseg_scaled_size(first_width,
                                             scale_num,
                                             scale_den);
    const int second_right = second_left +
                             dseg_scaled_size(second_width,
                                              scale_num,
                                              scale_den);
    const int right = first_right > second_right ? first_right : second_right;
    const int first_origin = (container_width - (right - left)) / 2 - left;
    return {first_origin, first_origin + second_origin};
}

class DirtyRanges {
public:
    void add(int x1, int x2)
    {
        x1 = std::max(0, x1 & ~1);
        x2 = std::min(kDisplayWidth - 1, x2 | 1);
        if (x1 > x2) {
            return;
        }
        RefreshRange incoming{x1, x2};
        for (int i = 0; i < count_;) {
            if (incoming.x1 > ranges_[i].x2 + 1 ||
                incoming.x2 < ranges_[i].x1 - 1) {
                ++i;
                continue;
            }
            incoming.x1 = std::min(incoming.x1, ranges_[i].x1);
            incoming.x2 = std::max(incoming.x2, ranges_[i].x2);
            ranges_[i] = ranges_[--count_];
            i = 0;
        }
        if (count_ < static_cast<int>(sizeof(ranges_) / sizeof(ranges_[0]))) {
            ranges_[count_++] = incoming;
        }
    }

    void flush(DisplayPort &display)
    {
        for (int i = 0; i < count_; ++i) {
            display.RLCD_DisplayXRange(ranges_[i].x1, ranges_[i].x2);
        }
    }

private:
    RefreshRange ranges_[8] = {};
    int count_ = 0;
};

void set_pixel(DisplayPort &display, int x, int y, bool black)
{
    if (x < 0 || y < 0 || x >= kDisplayWidth || y >= kDisplayHeight) {
        return;
    }
    display.RLCD_SetPixel(static_cast<uint16_t>(x),
                          static_cast<uint16_t>(y),
                          black ? ColorBlack : ColorWhite);
}

void fill_rect(DisplayPort &display, int x, int y, int w, int h, bool black)
{
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            set_pixel(display, px, py, black);
        }
    }
}

void fill_round_rect(DisplayPort &display,
                     int x,
                     int y,
                     int w,
                     int h,
                     int radius,
                     bool black)
{
    const int r2 = radius * radius;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            const int dx = px < radius
                               ? radius - 1 - px
                               : (px >= w - radius ? px - (w - radius) : 0);
            const int dy = py < radius
                               ? radius - 1 - py
                               : (py >= h - radius ? py - (h - radius) : 0);
            if (dx == 0 || dy == 0 || dx * dx + dy * dy <= r2) {
                set_pixel(display, x + px, y + py, black);
            }
        }
    }
}

uint32_t utf8_next(const char **cursor)
{
    const auto *p = reinterpret_cast<const uint8_t *>(*cursor);
    if (!p || *p == 0) {
        return 0;
    }
    uint32_t code = 0;
    int length = 1;
    if ((p[0] & 0x80U) == 0) {
        code = p[0];
    } else if ((p[0] & 0xE0U) == 0xC0U) {
        code = p[0] & 0x1FU;
        length = 2;
    } else if ((p[0] & 0xF0U) == 0xE0U) {
        code = p[0] & 0x0FU;
        length = 3;
    } else if ((p[0] & 0xF8U) == 0xF0U) {
        code = p[0] & 0x07U;
        length = 4;
    } else {
        code = '?';
    }
    for (int i = 1; i < length; ++i) {
        if ((p[i] & 0xC0U) != 0x80U) {
            code = '?';
            length = 1;
            break;
        }
        code = (code << 6U) | (p[i] & 0x3FU);
    }
    *cursor += length;
    return code;
}

const PowerGlyph *find_glyph(const PowerFont *font, uint32_t codepoint)
{
    if (!font || codepoint < font->unicode_base) {
        return nullptr;
    }
    const uint32_t offset = codepoint - font->unicode_base;
    for (uint16_t i = 0; i < font->glyph_count; ++i) {
        if (font->unicode_offsets[i] == offset) {
            return &font->glyphs[i];
        }
        if (font->unicode_offsets[i] > offset) {
            break;
        }
    }
    return nullptr;
}

int glyph_advance(const PowerFont *font,
                  const PowerGlyph *glyph,
                  const PowerGlyph *next_glyph)
{
    if (!font || !glyph) {
        return 0;
    }
    int kern = 0;
    if (font->kern_pairs && next_glyph) {
        const ptrdiff_t glyph_index = glyph - font->glyphs;
        const ptrdiff_t next_index = next_glyph - font->glyphs;
        if (glyph_index >= 0 && next_index >= 0 &&
            glyph_index < font->glyph_count && next_index < font->glyph_count) {
            kern = font->kern_pairs[glyph_index * font->glyph_count + next_index];
        }
    }
    return (glyph->adv_w + kern + 8) >> 4;
}

int text_width(const PowerFont *font, const char *text)
{
    int width = 0;
    const char *cursor = text;
    while (cursor && *cursor) {
        const uint32_t letter = utf8_next(&cursor);
        const char *next_cursor = cursor;
        const uint32_t next_letter = next_cursor && *next_cursor
                                         ? utf8_next(&next_cursor)
                                         : 0;
        width += glyph_advance(font,
                               find_glyph(font, letter),
                               find_glyph(font, next_letter));
    }
    return width;
}

void draw_text(DisplayPort &display,
               const PowerFont *font,
               int x,
               int line_y,
               const char *text,
               bool foreground_black)
{
    const char *cursor = text;
    int pen_x = x;
    while (cursor && *cursor) {
        const uint32_t letter = utf8_next(&cursor);
        const PowerGlyph *glyph = find_glyph(font, letter);
        if (!glyph) {
            continue;
        }
        const char *next_cursor = cursor;
        const uint32_t next_letter = next_cursor && *next_cursor
                                         ? utf8_next(&next_cursor)
                                         : 0;
        const PowerGlyph *next_glyph = find_glyph(font, next_letter);
        const uint8_t *bitmap = font->bitmap + glyph->bitmap_index;
        const int glyph_x = pen_x + glyph->ofs_x;
        const int glyph_y = line_y + (font->line_height - font->base_line) -
                            glyph->box_h - glyph->ofs_y;
        if (bitmap) {
            for (int py = 0; py < glyph->box_h; ++py) {
                for (int px = 0; px < glyph->box_w; ++px) {
                    const uint32_t bit =
                        static_cast<uint32_t>(py) * glyph->box_w + px;
                    if ((bitmap[bit >> 3U] & (0x80U >> (bit & 7U))) != 0) {
                        set_pixel(display,
                                  glyph_x + px,
                                  glyph_y + py,
                                  foreground_black);
                    }
                }
            }
        }
        pen_x += glyph_advance(font, glyph, next_glyph);
    }
}

void draw_text_centered(DisplayPort &display,
                        const PowerFont *font,
                        int area_x,
                        int area_w,
                        int line_y,
                        const char *text,
                        bool foreground_black)
{
    const int width = text_width(font, text);
    draw_text(display,
              font,
              area_x + (area_w - width) / 2,
              line_y,
              text,
              foreground_black);
}

const DsegGlyph *digit_glyph(int digit)
{
    return digit >= 0 && digit <= 9 ? &kDSEG84Glyphs[digit] : nullptr;
}

bool packed_bit(const uint8_t *bitmap, uint32_t bit)
{
    return bitmap && (bitmap[bit >> 3U] & (0x80U >> (bit & 7U))) != 0;
}

void draw_scaled_digit(DisplayPort &display,
                       int card_x,
                       int origin_x,
                       const DsegGlyph *glyph)
{
    constexpr int scale_num = 3;
    constexpr int scale_den = 4;
    constexpr int baseline_y = 84;
    if (!glyph) {
        return;
    }
    const int dst_w = (glyph->width * scale_num + scale_den - 1) / scale_den;
    const int dst_h = (glyph->height * scale_num + scale_den - 1) / scale_den;
    const int dst_x = card_x + origin_x + glyph->x_offset * scale_num / scale_den;
    const int dst_y = kCardY + baseline_y + glyph->y_offset * scale_num / scale_den;
    for (int y = 0; y < dst_h; ++y) {
        const int src_y = y * glyph->height / dst_h;
        for (int x = 0; x < dst_w; ++x) {
            const int src_x = x * glyph->width / dst_w;
            const uint32_t bit = static_cast<uint32_t>(src_y) * glyph->width + src_x;
            if (packed_bit(kDSEG84Font.bitmap + glyph->bitmap_offset, bit)) {
                set_pixel(display, dst_x + x, dst_y + y, false);
            }
        }
    }
}

void draw_two_digits(DisplayPort &display, int card_x, int value)
{
    const DsegGlyph *first = digit_glyph(value / 10);
    const DsegGlyph *second = digit_glyph(value % 10);
    if (!first || !second) {
        return;
    }
    const DsegPairLayout layout = centered_dseg_pair_layout(kCardW,
                                                            3,
                                                            4,
                                                            first->x_offset,
                                                            first->width,
                                                            first->x_advance,
                                                            second->x_offset,
                                                            second->width);
    draw_scaled_digit(display, card_x, layout.first_origin_x, first);
    draw_scaled_digit(display, card_x, layout.second_origin_x, second);
}

constexpr uint8_t kLetterA[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
constexpr uint8_t kLetterM[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
constexpr uint8_t kLetterP[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};

void draw_block_letter(DisplayPort &display,
                       int x,
                       int y,
                       const uint8_t rows[7],
                       int scale)
{
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1U << (4 - col))) != 0) {
                fill_rect(display,
                          x + col * scale,
                          y + row * scale,
                          scale,
                          scale,
                          false);
            }
        }
    }
}

void draw_am_pm(DisplayPort &display, bool pm)
{
    constexpr int scale = 8;
    constexpr int gap = 8;
    constexpr int pair_w = 5 * scale * 2 + gap;
    constexpr int pair_h = 7 * scale;
    const int x = kCardX[0] + (kCardW - pair_w) / 2;
    const int y = kCardY + (kCardH - pair_h) / 2;
    draw_block_letter(display, x, y, pm ? kLetterP : kLetterA, scale);
    draw_block_letter(display, x + 5 * scale + gap, y, kLetterM, scale);
}

void draw_bitmap(DisplayPort &display,
                 int x,
                 int y,
                 int width,
                 int height,
                 int bytes_per_row,
                 const uint8_t *bits,
                 bool foreground_black)
{
    if (!bits) {
        return;
    }
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const uint8_t value = bits[py * bytes_per_row + px / 8];
            if ((value & (0x80U >> (px & 7))) != 0) {
                set_pixel(display, x + px, y + py, foreground_black);
            }
        }
    }
}

const uint8_t *temperature_mood_bits(float value)
{
    if (value >= 20.0f && value <= 26.0f) {
        return flip_temp_comfort_icon_bits;
    }
    if (value >= 18.0f && value <= 30.0f) {
        return flip_temp_ok_icon_bits;
    }
    return flip_temp_bad_icon_bits;
}

const uint8_t *humidity_mood_bits(float value)
{
    if (value >= 40.0f && value <= 60.0f) {
        return flip_humi_comfort_icon_bits;
    }
    if (value >= 30.0f && value <= 70.0f) {
        return flip_humi_ok_icon_bits;
    }
    return flip_humi_bad_icon_bits;
}

void draw_battery(DisplayPort &display, int segments)
{
    fill_round_rect(display,
                    kBatteryFrameX,
                    kBatteryFrameY,
                    kBatteryFrameW,
                    kBatteryFrameH,
                    3,
                    true);
    fill_round_rect(display,
                    kBatteryFrameX + 2,
                    kBatteryFrameY + 2,
                    30,
                    12,
                    2,
                    false);
    fill_round_rect(display, kBatteryTipX, kBatteryTipY, 3, 6, 1, true);
    for (int i = 0; i < kBatterySegmentCount; ++i) {
        if (i < segments) {
            fill_round_rect(display,
                            kBatteryFrameX + 3 + i * 6,
                            kBatteryFrameY + 4,
                            4,
                            8,
                            1,
                            true);
        }
    }
}

void draw_progress(DisplayPort &display, int filled)
{
    filled = std::clamp(filled, 0, kProgressSegments);
    for (int i = 0; i < kProgressSegments; ++i) {
        const int x0 = kProgressX + i * kProgressStride;
        for (int y = 0; y < kProgressSegmentH; ++y) {
            for (int x = 0; x < kProgressSegmentW; ++x) {
                const bool border = x == 0 || x == kProgressSegmentW - 1 ||
                                    y == 0 || y == kProgressSegmentH - 1;
                set_pixel(display, x0 + x, kProgressY + y, i < filled || border);
            }
        }
    }
}

void draw_header(DisplayPort &display,
                 const struct tm &local,
                 int battery_segments,
                 bool sound_enabled,
                 int progress_filled)
{
    draw_battery(display, battery_segments);
    if (sound_enabled) {
        draw_bitmap(display,
                    kChimeX,
                    kChimeY,
                    kChimeW,
                    kChimeH,
                    kChimeBytesPerRow,
                    kChimeBits,
                    true);
    }
    const char *weekday = local.tm_wday >= 0 && local.tm_wday < 7
                              ? kWeekdayNames[local.tm_wday]
                              : "星期日";
    char date[40] = {};
    std::snprintf(date,
                  sizeof(date),
                  "%04d/%02d/%02d / %s",
                  local.tm_year + 1900,
                  local.tm_mon + 1,
                  local.tm_mday,
                  weekday);
    const int width = text_width(&power_font_16, date);
    draw_text(display,
              &power_font_16,
              380 - width,
              15,
              date,
              true);
    draw_progress(display, progress_filled);
}

void draw_sensor_panel(DisplayPort &display, const PowerSensorReading &sensor)
{
    fill_round_rect(display,
                    kSensorPanelX,
                    kSensorPanelY,
                    kSensorPanelW,
                    kSensorPanelH,
                    kSensorPanelRadius,
                    true);
    char temperature[16] = "--.-C";
    char humidity[16] = "--%";
    if (sensor.available) {
        std::snprintf(temperature, sizeof(temperature), "%.1fC", sensor.temperature);
        std::snprintf(humidity, sizeof(humidity), "%.0f%%", sensor.humidity);
    }
    // 字体子集已预合成完整版的正常、右移 1px、下移 1px 三层叠字。
    draw_text(display, &power_font_24, 34, 204, temperature, false);
    draw_text(display, &power_font_24, 34, 243, humidity, false);

    if (sensor.available) {
        draw_bitmap(display,
                    200,
                    204,
                    FLIP_SENSOR_ICON_WIDTH,
                    FLIP_SENSOR_ICON_HEIGHT,
                    FLIP_SENSOR_ICON_BYTES_PER_ROW,
                    temperature_mood_bits(sensor.temperature),
                    false);
        draw_bitmap(display,
                    200,
                    244,
                    FLIP_SENSOR_ICON_WIDTH,
                    FLIP_SENSOR_ICON_HEIGHT,
                    FLIP_SENSOR_ICON_BYTES_PER_ROW,
                    humidity_mood_bits(sensor.humidity),
                    false);
    }
}

void draw_date_panel(DisplayPort &display, const struct tm &local)
{
    fill_round_rect(display,
                    kDatePanelX,
                    kDatePanelY,
                    kDatePanelW,
                    kDatePanelH,
                    kDatePanelRadius,
                    true);
    char day[8] = {};
    std::snprintf(day, sizeof(day), "%d", local.tm_mday);
    draw_text_centered(display,
                       &power_digits_48,
                       kDatePanelX,
                       kDatePanelW,
                       196,
                       day,
                       false);
    CalendarDayInfo lunar = {};
    const char *subtext = calendar_day_info(local, &lunar) && lunar.subtext[0]
                              ? lunar.subtext
                              : "--";
    draw_text_centered(display,
                       &power_lunar_24,
                       kDatePanelX,
                       kDatePanelW,
                       249,
                       subtext,
                       false);
}

int battery_segment_count(int percent)
{
    if (percent < 0) {
        return 0;
    }
    return std::clamp((percent + 19) / 20, 0, kBatterySegmentCount);
}

} // namespace

PowerUiSnapshot power_ui_snapshot(const struct tm &local,
                                  const PowerSensorReading &sensor,
                                  int battery_percent,
                                  bool sound_enabled)
{
    PowerUiSnapshot snapshot = {};
    snapshot.magic = kPowerUiSnapshotMagic;
    snapshot.date_key = (local.tm_year + 1900) * 10000 +
                        (local.tm_mon + 1) * 100 + local.tm_mday;
    snapshot.am_pm = local.tm_hour >= 12 ? 1 : 0;
    snapshot.hour12 = local.tm_hour % 12;
    if (snapshot.hour12 == 0) {
        snapshot.hour12 = 12;
    }
    snapshot.minute = local.tm_min;
    snapshot.temperature_tenths = sensor.available
                                      ? static_cast<int>(std::lround(sensor.temperature * 10.0f))
                                      : INT32_MIN;
    snapshot.humidity_percent = sensor.available
                                    ? static_cast<int>(std::lround(sensor.humidity))
                                    : INT32_MIN;
    snapshot.battery_segments = battery_segment_count(battery_percent);
    const int day_seconds = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    snapshot.progress_filled = day_seconds * kProgressSegments / (24 * 3600);
    snapshot.sound_enabled = sound_enabled;
    return snapshot;
}

void power_ui_render(DisplayPort &display,
                     const struct tm &local,
                     const PowerSensorReading &sensor,
                     int battery_percent,
                     bool sound_enabled)
{
    const PowerUiSnapshot snapshot = power_ui_snapshot(local,
                                                       sensor,
                                                       battery_percent,
                                                       sound_enabled);
    display.RLCD_ColorClear(ColorWhite);
    draw_header(display,
                local,
                snapshot.battery_segments,
                sound_enabled,
                snapshot.progress_filled);
    fill_rect(display,
              kTopSeparatorX,
              kTopSeparatorY,
              kTopSeparatorW,
              kTopSeparatorH,
              true);
    for (int i = 0; i < kCardCount; ++i) {
        fill_round_rect(display,
                        kCardX[i],
                        kCardY,
                        kCardW,
                        kCardH,
                        kCardRadius,
                        true);
    }
    draw_am_pm(display, snapshot.am_pm != 0);
    draw_two_digits(display, kCardX[1], snapshot.hour12);
    draw_two_digits(display, kCardX[2], snapshot.minute);
    draw_sensor_panel(display, sensor);
    draw_date_panel(display, local);
}

void power_ui_refresh(DisplayPort &display,
                      const PowerUiSnapshot &previous,
                      const PowerUiSnapshot &current,
                      bool force_full)
{
    if (force_full || previous.magic != kPowerUiSnapshotMagic ||
        current.date_key != previous.date_key) {
        display.RLCD_Display();
        return;
    }

    DirtyRanges dirty;
    if (current.sound_enabled != previous.sound_enabled) {
        dirty.add(kChimeX, kChimeX + kChimeW - 1);
    }
    if (current.am_pm != previous.am_pm) {
        dirty.add(kCardX[0], kCardX[0] + kCardW - 1);
    }
    if (current.hour12 != previous.hour12) {
        dirty.add(kCardX[1], kCardX[1] + kCardW - 1);
    }
    if (current.minute != previous.minute) {
        dirty.add(kCardX[2], kCardX[2] + kCardW - 1);
    }
    if (current.temperature_tenths != previous.temperature_tenths ||
        current.humidity_percent != previous.humidity_percent) {
        dirty.add(kSensorPanelX, kSensorPanelX + kSensorPanelW - 1);
    }
    if (current.battery_segments != previous.battery_segments) {
        dirty.add(kBatteryFrameX, kBatteryTipX + 2);
    }
    if (current.progress_filled != previous.progress_filled) {
        if (current.progress_filled > previous.progress_filled) {
            const int segment = current.progress_filled - 1;
            dirty.add(kProgressX + segment * kProgressStride,
                      kProgressX + segment * kProgressStride + kProgressSegmentW - 1);
        } else {
            dirty.add(kProgressX,
                      kProgressX + (kProgressSegments - 1) * kProgressStride +
                          kProgressSegmentW - 1);
        }
    }
    dirty.flush(display);
}
