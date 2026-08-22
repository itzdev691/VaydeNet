cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../packages/platforms/esp32"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(vaydenet_node)
