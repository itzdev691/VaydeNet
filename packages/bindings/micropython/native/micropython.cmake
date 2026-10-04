add_library(usermod_vaydenet INTERFACE)
get_filename_component(VAYDENET_REPO "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
target_sources(usermod_vaydenet INTERFACE ${CMAKE_CURRENT_LIST_DIR}/modvaydenet.c)
# Keep C++ constructors and engine state in one archive. ESP-IDF propagates
# INTERFACE sources to both the main component and final executable.
add_library(vaydenet_native_core STATIC
    ${CMAKE_CURRENT_LIST_DIR}/bridge.cpp
    ${VAYDENET_REPO}/packages/VaydeEngine/src/VaydeEngine.cpp
    ${VAYDENET_REPO}/packages/VaydeEngine/src/PacketValidation.cpp
    ${VAYDENET_REPO}/packages/VaydeEngine/src/PacketMessageDecoder.cpp
    ${VAYDENET_REPO}/packages/VaydeEngine/src/PacketMessageEncoder.cpp
)
target_include_directories(usermod_vaydenet INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(vaydenet_native_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${VAYDENET_REPO}/packages/VaydeEngine/include
)
target_compile_features(vaydenet_native_core PUBLIC cxx_std_17)
target_compile_options(vaydenet_native_core PRIVATE -fno-exceptions -fno-rtti)
target_link_libraries(usermod_vaydenet INTERFACE vaydenet_native_core)
target_link_libraries(usermod INTERFACE usermod_vaydenet)
