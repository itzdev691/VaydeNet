cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../packages/VaydeEngine"
    "${CMAKE_CURRENT_LIST_DIR}/../../packages/platforms/esp32"
    "${CMAKE_CURRENT_LIST_DIR}/../../packages/adapters/esp-now"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(vaydenet_node)
