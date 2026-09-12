// 封装 SDL 天气时钟主页的对象、状态切换和秒级预览刷新。
#pragma once

#include <array>
#include <cstddef>
#include <time.h>
#include <vector>

#include "lvgl.h"
#include "sdl_preview_progress.h"
#include "sdl_preview_work_status.h"

class SdlPreviewClock {
public:
    explicit SdlPreviewClock(sdl_preview_work_status::Bar &work_status);

    void build(lv_obj_t *screen);
    void populate_sample_data();
    void apply_low_battery(bool low);
    void apply_alert(bool visible);
    void show_setup_status();
    void update_time(const struct tm &local);
    void update_battery(int percent);

private:
    static constexpr std::size_t kLowerPanelObjectCapacity = 12;

    void prepare_screen(lv_obj_t *screen);
    void build_status_header(lv_obj_t *screen);
    void build_weather_summary(lv_obj_t *screen);
    void build_sensor_summary(lv_obj_t *screen);
    void build_time_and_progress(lv_obj_t *screen);
    void build_setup_status(lv_obj_t *screen);
    void draw_time(const struct tm &local);
    void draw_second(const struct tm &local);
    void draw_status_gif_frame(int frame);
    void update_trend_icon(lv_obj_t *canvas, int trend);
    void remember_lower_panel_object(lv_obj_t *obj);
    void set_lower_panel_visible(bool visible);
    void set_setup_panel_visible(bool visible);
    static void set_object_visible(lv_obj_t *obj, bool visible);

    sdl_preview_work_status::Bar &work_status_;
    lv_obj_t *temp_icon_canvas_ = nullptr;
    lv_obj_t *humi_icon_canvas_ = nullptr;
    lv_obj_t *temp_label_ = nullptr;
    lv_obj_t *humi_label_ = nullptr;
    lv_obj_t *temp_trend_canvas_ = nullptr;
    lv_obj_t *humi_trend_canvas_ = nullptr;
    lv_obj_t *weather_city_label_ = nullptr;
    lv_obj_t *weather_info_label_ = nullptr;
    lv_obj_t *weather_icon_label_ = nullptr;
    lv_obj_t *weather_temp_label_ = nullptr;
    lv_obj_t *weather_humi_label_ = nullptr;
    lv_obj_t *alert_pill_ = nullptr;
    lv_obj_t *alert_icon_canvas_ = nullptr;
    lv_obj_t *alert_label_ = nullptr;
    lv_obj_t *low_battery_icon_canvas_ = nullptr;
    lv_obj_t *panel_sep_a_ = nullptr;
    lv_obj_t *panel_sep_b_ = nullptr;
    lv_obj_t *time_canvas_ = nullptr;
    lv_obj_t *second_canvas_ = nullptr;
    lv_obj_t *status_gif_canvas_ = nullptr;
    std::array<lv_obj_t *, kLowerPanelObjectCapacity> lower_panel_objects_{};
    std::size_t lower_panel_object_count_ = 0;
    std::array<lv_obj_t *, 6> setup_status_labels_{};
    int last_status_gif_frame_ = -1;
    int last_second_ = -1;
    int last_minute_ = -1;
    std::vector<lv_color_t> time_canvas_pixels_;
    std::vector<lv_color_t> second_canvas_pixels_;
    std::vector<lv_color_t> status_gif_canvas_pixels_;
    std::vector<lv_color_t> alert_icon_canvas_pixels_;
    std::vector<lv_color_t> low_battery_icon_canvas_pixels_;
    std::vector<lv_color_t> temp_trend_canvas_pixels_;
    std::vector<lv_color_t> humi_trend_canvas_pixels_;
    std::vector<lv_color_t> temp_icon_canvas_pixels_;
    std::vector<lv_color_t> humi_icon_canvas_pixels_;
    sdl_preview_progress::Canvas day_progress_;
    sdl_preview_progress::Canvas second_progress_;
};
