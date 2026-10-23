
cmake_minimum_required(VERSION 3.16.0)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "C:/SCHOOL_WORKS/BCA152/esp-rainmaker/components/esp_rainmaker"
    "C:/SCHOOL_WORKS/BCA152/esp-rainmaker/examples/common/rmaker_app_reset"
    "C:/SCHOOL_WORKS/BCA152/esp-rainmaker/examples/common/rmaker_app_network"
    "C:/SCHOOL_WORKS/BCA152/esp-rainmaker/examples/common/rmaker_app_insights"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(smart_switch)
