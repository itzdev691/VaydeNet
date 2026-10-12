# Native-radio targets use ESP-IDF's own ESP-NOW implementation.
if(NOT IDF_TARGET STREQUAL "esp32p4")
    idf_component_register()
    return()
endif()

set(bridge_source_dir "${CMAKE_CURRENT_LIST_DIR}/../../third_party/esp-now-hosted/host")

idf_component_register(
    SRCS "EspNowHosted.cpp"
    INCLUDE_DIRS "${bridge_source_dir}"
    PRIV_REQUIRES
        espressif__esp_hosted
        espressif__esp_wifi_remote
        esp_wifi
        esp_timer
        freertos
        log
)
