# 配置天气时钟 ESP-IDF 工程版本、项目名和额外组件目录。
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "v1.6.5")
# EXTRA_COMPONENT_DIRS 必须在 include(project.cmake) 之前设置：IDF 在那一步
# 扫描组件目录，之后再改不生效。
set(EXTRA_COMPONENT_DIRS
    components/ExternLib/SensorLib
    components/ExternLib/esp_codec_dev
    components/ExternLib/codec_board
    # airkan 遥控协议 SDK（独立仓库，Apache-2.0）。指向外部目录而非复制，
    # 避免工程内出现两份实现。其组件名为 airkan。
    E:/ESP/airkan-sdk)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weather_clock)

# Keep the object arena outside scarce internal/DMA RAM, matching SDL's budget.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/components/app_bsp")
target_compile_definitions(${lvgl_lib} PRIVATE
    LV_MEM_SIZE=262144
    LV_MEM_POOL_INCLUDE="lvgl_memory_pool.h"
    LV_MEM_POOL_ALLOC=weather_clock_lvgl_pool)
