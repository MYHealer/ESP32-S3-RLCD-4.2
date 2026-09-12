cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# 只解析 main 的依赖闭包，IDF 未使用组件不参与配置和编译。
idf_build_set_property(MINIMAL_BUILD ON)
project(power_demo)
